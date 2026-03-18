#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

volatile __sig_atomic_t run = 1;
pthread_mutex_t sigmtx = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int i) {
    pthread_mutex_lock(&sigmtx);
    run = 0;
    pthread_mutex_unlock(&sigmtx);
}

typedef struct shelf {
    int v;
    pthread_mutex_t mx;
}shelf_t;

typedef struct common {
    shelf_t* shelf;
    pthread_mutex_t mxShelf;
    int product_count;
}common_t;

typedef struct worker_args {
    int idx;
    unsigned int seed;
    common_t* c;
}worker_t;

void read_args(int argc, char** argv, int* product_count, int* worker_count);
void start_nightshift(int worker_count, pthread_t* wids, worker_t* wargs, common_t* c);
void* worker_shift(void* wargs);
void end_nightshift(pthread_t* wids, int worker_count);
void init_shelf(shelf_t* shelf, int product_count);
void print_shelf(shelf_t* shelf, int product_count);
void swap(int* v1, int* v2);
void msleep(unsigned int milisec);
void shuffle(shelf_t* shelf, int product_count);
void sethandler(void (*f)(int), int sigNo);

void usage(char* argv) {
    fprintf(stderr, "Usage: %s N M\n", argv);
    fprintf(stderr, "\t8<= N <= 256: no. of products\n");
    fprintf(stderr, "\t1 <= M <= 16: no. of workers\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    int product_count, worker_count;//, err;
    int signum;
    read_args(argc, argv, &product_count, &worker_count);

    shelf_t* shelf = malloc(sizeof(shelf_t) * product_count);
    if (shelf == NULL) ERR("malloc");

    init_shelf(shelf, product_count);
    common_t c;
    pthread_mutex_init(&c.mxShelf, NULL);
    c.shelf = shelf;
    c.product_count = product_count;
    pthread_t wids[worker_count];
    worker_t wargs[worker_count];

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR1);
    sethandler(sigint_handler, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
        ERR("pthread_sigmask");

    start_nightshift(worker_count, wids, wargs, &c);

    if (pthread_sigmask(SIG_UNBLOCK, &mask, NULL) != 0)
        ERR("pthread_sigmask");

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
        ERR("pthread_sigmask");

    alarm(1);
    while (1) {
        pthread_mutex_lock(&sigmtx);
        int run_copy = run;
        pthread_mutex_unlock(&sigmtx);
        if (!run_copy) break;

        sigwait(&mask, &signum);
        if (signum == SIGALRM) {
            print_shelf(shelf, product_count);
        }
        else if (signum == SIGUSR1) {
            shuffle(shelf, product_count);
        }
        else{}
        alarm(1);
    }
    printf("Ending nightshift...\n");
    end_nightshift(wids, worker_count);

    printf("Last print and free'ing...\n");
    print_shelf(shelf, product_count);
    free(shelf);
    return EXIT_SUCCESS;
}

void read_args(int argc, char** argv, int* product_count, int* worker_count) {
    if (argc != 3) usage(argv[0]);
    *product_count = atoi(argv[1]);
    *worker_count = atoi(argv[2]);
    if (*product_count < 8 || *product_count > 256) usage(argv[0]);
    if (*worker_count < 1 || *worker_count > 16) usage(argv[0]);
}

void start_nightshift(int worker_count, pthread_t* wids, worker_t* wargs, common_t* c) {
    int err;
    srand(getpid());
    for (int i = 0; i < worker_count; i++) {
        wargs[i].idx = i + 1;
        wargs[i].c = c;
        wargs[i].seed = rand();
        err = pthread_create(&wids[i], NULL, worker_shift, &wargs[i]);
        if (err != 0) ERR("pthread_create");
    }
}

void* worker_shift(void* wargs) {
    worker_t* w = (worker_t*) wargs;
    printf("W[%d] Reporting for the night shift!\n", w->idx);
    int idx1 = -1, idx2 = -1;
    while (1) {
        pthread_mutex_lock(&sigmtx);
        int run_copy = run;
        pthread_mutex_unlock(&sigmtx);

        if (!run_copy) break;

        do {
            idx1 = rand_r(&w->seed) % w->c->product_count;
            idx2 = rand_r(&w->seed) % w->c->product_count;
        } while (idx1 == idx2);
        if (idx1 > idx2) swap(&idx1, &idx2);

        pthread_mutex_lock(&w->c->shelf[idx1].mx);
        pthread_mutex_lock(&w->c->shelf[idx2].mx);
        if (w->c->shelf[idx1].v > w->c->shelf[idx2].v)
            swap(&w->c->shelf[idx1].v, &w->c->shelf[idx2].v);
        pthread_mutex_unlock(&w->c->shelf[idx2].mx);
        pthread_mutex_unlock(&w->c->shelf[idx1].mx);
        // printf("W[%d] i = %d!\n", w->idx, i);
        msleep(100);
    }
    // printf("bye!\n");
    return NULL;
}

void end_nightshift(pthread_t* wids, int worker_count) {
    int err;
    for (int i = 0; i < worker_count; i++) {
        err = pthread_join(wids[i], NULL);
        if (err != 0) ERR("pthread_join");
    }
}

void init_shelf(shelf_t* shelf, int product_count) {
    srand(getpid());
    for (int i = 0; i < product_count; i++) {
        int val = i + 1; //rand() % product_count;
        shelf[i].v = val;
        pthread_mutex_init(&shelf[i].mx, NULL);
    }
    shuffle(shelf, product_count);
}

void print_shelf(shelf_t* shelf, int product_count) {
    printf("[ ");
    for (int i = 0; i < product_count; i++) {
        pthread_mutex_lock(&shelf[i].mx);
    }
    for (int i = 0; i < product_count; i++) {
        printf("%d ", shelf[i].v);
    }
    for (int i = 0; i < product_count; i++) {
        pthread_mutex_unlock(&shelf[i].mx);
    }
    printf("]\n");
}

void swap(int* v1, int* v2) {
    int temp;
    temp = *v1;
    *v1 = *v2;
    *v2 = temp;
}

void msleep(unsigned int milisec) {
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    nanosleep(&req, NULL);
}

void shuffle(shelf_t* shelf, int product_count) {
    srand(getpid());
    int idx1 = -1, idx2 = -1;

    for (int i = 0; i < product_count; i++) {
        pthread_mutex_lock(&shelf[i].mx);
    }

    for (int i = 0; i < 100 * product_count; i++) {
        do {
            idx1 = rand() % product_count;
            idx2 = rand() % product_count;
        }while (idx1 == idx2);

        if (idx1 > idx2) swap(&idx1, &idx2);
        swap(&shelf[idx1].v, &shelf[idx2].v);
    }

    for (int i = 0; i < product_count; i++) {
        pthread_mutex_unlock(&shelf[i].mx);
    }
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}