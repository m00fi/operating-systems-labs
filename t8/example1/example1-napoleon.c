#include "l8_common.h"

#define BUFSIZE 256
#define MAXNAME 128
#define STACK_SIZE 16
#define DIVISION_NAMES_SIZE 128

typedef struct message {
    char text[BUFSIZE + 1];
    struct sockaddr_in addr;
} msg_t;

typedef struct adiutant {
    msg_t stack[STACK_SIZE];
    int top;
    pthread_cond_t cond;
    pthread_mutex_t mutex;

    char division_names[DIVISION_NAMES_SIZE][MAXNAME + 1];
    pthread_mutex_t division_mutex;

    int map[100][100];
    pthread_mutex_t map_mutex[100];

    struct sockaddr_in addresses[DIVISION_NAMES_SIZE];

    int is_allied[DIVISION_NAMES_SIZE];
    int fd;
} shared_t;

void usage(char* name) {
    fprintf(stderr, "%s port\n", name);
    exit(EXIT_FAILURE);
}

int parse_and_print(char* buf, char* division, int* out_x, int* out_y, int* out_p) {
    int x,y,p;
    char name[MAXNAME + 1];

    int count = sscanf(buf, "%d %d %d %128[^\n]",&x, &y, &p, name);
    if (count != 4) {
        printf("Błędny format wiadomości!\n");
        return -1;
    }
    else if (x < 0 || x > 99 || y < 0 || y > 99) {
        printf("Błędny format wiadomości!\n");
        return -1;
    }
    else if (p != 0 && p != 1) {
        printf("Błędny format wiadomości!\n");
        return -1;
    }
    else if (p == 0) {
        printf("Wrogi oddział %s widziany na pozycji %d:%d.\n", name, x, y);
    }
    else {
        printf("Nasz oddział %s widziany na pozycji %d:%d.\n", name, x, y);
    }
    memcpy(division, name, MAXNAME + 1);
    *out_x = x;
    *out_y = y;
    *out_p = p;
    return 0;
}

int put_division(shared_t* sh, char* division) {
    char comp[MAXNAME + 1];
    memset(comp, 0, MAXNAME + 1);

    pthread_mutex_lock(&sh->division_mutex);
    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        if (strcmp(division, sh->division_names[i]) == 0) {
            pthread_mutex_unlock(&sh->division_mutex);
            return i;
        }
        if (memcmp(comp, sh->division_names[i], MAXNAME + 1) == 0) {
            memcpy(sh->division_names[i], division, MAXNAME + 1);
            pthread_mutex_unlock(&sh->division_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&sh->division_mutex);
    return -1;
}

void move_division(shared_t* sh, char* division, int idx, int x, int y) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&sh->map_mutex[i]);
        for (int j = 0; j < 100; j++) {
            if (sh->map[i][j] == idx) {
                sh->map[i][j] = -1;
            }
        }
        if (i == x)
            sh->map[x][y] = idx;
        pthread_mutex_unlock(&sh->map_mutex[i]);
    }
}

void* thread_work(void* shared) {
    shared_t* sh = (shared_t*) shared;
    char division[MAXNAME + 1];
    int x, y;
    msg_t current_msg;

    while (1) {
        pthread_mutex_lock(&sh->mutex);

        while (sh->top == -1)
            pthread_cond_wait(&sh->cond, &sh->mutex);

        current_msg = sh->stack[sh->top];
        sh->top--;

        pthread_mutex_unlock(&sh->mutex);
        usleep(10000);

        int p;
        int ret = parse_and_print(current_msg.text, division, &x, &y, &p);
        if (ret == -1) continue;

        int idx = put_division(sh, division);
        if (idx == -1) continue;

        pthread_mutex_lock(&sh->division_mutex);
        sh->addresses[idx] = current_msg.addr;
        sh->is_allied[idx] = p;
        pthread_mutex_unlock(&sh->division_mutex);

        move_division(sh, division, idx, x, y);
    }
    return NULL;
}

void print_map(shared_t* sh) {
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&sh->map_mutex[i]);
        for (int j = 0; j < 10; j++) {
            printf("%d", sh->map[i][j]);
        }
        printf("\n");
        pthread_mutex_unlock(&sh->map_mutex[i]);
    }
}

void* napoleon_work(void* shared) {
    shared_t* sh = (shared_t*) shared;
    char order[BUFSIZE + 1];

    srand(time(NULL));

    while (1) {
        usleep(3000000);

        printf("\n--- STAN MAPY ---\n");
        print_map(sh);

        int allied_indices[DIVISION_NAMES_SIZE];
        int allied_count = 0;

        pthread_mutex_lock(&sh->division_mutex);
        for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
            if (sh->division_names[i][0] != '\0' && sh->is_allied[i] == 1) {
                allied_indices[allied_count] = i;
                allied_count++;
            }
        }

        if (allied_count > 0) {
            int random_idx = allied_indices[rand() % allied_count];

            struct sockaddr_in target_addr = sh->addresses[random_idx];
            char target_name[MAXNAME + 1];
            strcpy(target_name, sh->division_names[random_idx]);

            pthread_mutex_unlock(&sh->division_mutex);

            int target_x = rand() % 100;
            int target_y = rand() % 100;

            int order_len = snprintf(order, BUFSIZE, "%d %d 1 %s", target_x, target_y, target_name);

            TEMP_FAILURE_RETRY(sendto(sh->fd, order, order_len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr)));

            printf("Napoleon wysłał rozkaz przemieszczenia na %d:%d do oddziału %s.\n", target_x, target_y, target_name);

        } else {
            pthread_mutex_unlock(&sh->division_mutex);
        }
    }
    return NULL;
}

void server_work(int fd, shared_t* shared) {
    char buf[BUFSIZE + 1];
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);

    while (1) {
        int receivedBytes;
        if ((receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&addr, &size))) < 0)
            ERR("read:");
        buf[receivedBytes] = '\0';

        pthread_mutex_lock(&shared->mutex);
        if (shared->top == 15) {
            pthread_mutex_unlock(&shared->mutex);
            pthread_cond_signal(&shared->cond);
            continue;
        }

        shared->top++;
        memcpy(shared->stack[shared->top].text, buf, BUFSIZE + 1);
        shared->stack[shared->top].addr = addr;

        pthread_mutex_unlock(&shared->mutex);
        pthread_cond_signal(&shared->cond);
    }
}

int main(int argc, char** argv) {
    if (argc != 2)
        usage(argv[0]);

    int fd;
    int port = atoi(argv[1]);

    shared_t shared;
    shared.top = -1;
    pthread_cond_init(&shared.cond, NULL);
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_mutex_init(&shared.division_mutex, NULL);

    for (int i = 0; i < 100; i++) {
        pthread_mutex_init(&shared.map_mutex[i], NULL);
        for (int j = 0; j < 100; j++) {
            shared.map[i][j] = -1;
        }
    }

    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        memset(shared.division_names[i], 0, MAXNAME + 1);
    }

    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Seting SIGPIPE:");
    fd = bind_inet_socket(port, SOCK_DGRAM);

    shared.fd = fd;
    pthread_t t[5];

    for (int i = 0; i < 4; i++) {
        pthread_create(&(t[i]), NULL, thread_work, &shared);
    }
    pthread_create(&(t[4]), NULL, napoleon_work, &shared);

    server_work(fd, &shared);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}