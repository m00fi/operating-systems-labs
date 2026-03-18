#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUF_SZ 16

#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void usage(char* name) {
    fprintf(stderr, "Usage: %s N M\n", name);
    fprintf(stderr, "2 ≤ N ≤ 5 - number of processes\n");
    fprintf(stderr, "5 ≤ M ≤ 10 - number of cards\n");
    exit(EXIT_FAILURE);
}

void parse_arguments(int argc, char** argv, int* N, int* M) {
    if (argc != 3)
        usage(argv[0]);
    *N = atoi(argv[1]);
    *M = atoi(argv[2]);

    if (*N < 2 || *N > 5 || *M < 1 || *M > 10)
        usage(argv[0]);
}

void player_work(int M, int read_fd, int write_fd, int idx) {
    srand(getpid());
    int random_n = rand() % (M + 1);
    char write_buf[BUF_SZ];
    char read_buf[BUF_SZ];
    memset(write_buf, '0', BUF_SZ);
    ssize_t status = 0;
    int chosen_card = -1;
    int cards[M];
    for (int i = 0; i < M; i++) {
        cards[i] = i + 1;
    }

    while ((status = read(read_fd, read_buf, BUF_SZ)) == BUF_SZ) {
        if (strcmp(read_buf, "new_round") == 0) {
            random_n = rand() % (M);
            while (cards[random_n] == -1) {
                random_n = rand() % (M);
            }
            chosen_card = cards[random_n];
            cards[random_n] = -1;

            memset(write_buf, '0', BUF_SZ);
            sprintf(write_buf, "%d", chosen_card);

            if ((status = write(write_fd, write_buf, BUF_SZ)) != BUF_SZ)
                ERR("write() failed");
        }

        // int ex = rand() % 100;
        // if (ex < 5) {
        //     exit(EXIT_SUCCESS);
        // }

        if ((status = read(read_fd, read_buf, BUF_SZ)) != BUF_SZ)
            ERR("read() failed");
        // int points = atoi(read_buf);
        // if (points > 0) {
        //     printf("Player %d: I WON %d!\n", idx, points);
        // }
    }
}

void server_work(int N, int M, int* player_fds, int* read_fds) {
    ssize_t status = 0;
    char read_buf[BUF_SZ];
    int random_number = 0;

    char write_buf[BUF_SZ];
    memset(write_buf, '0', BUF_SZ);
    char* round = "new_round";
    sprintf(write_buf, "%s", round);
    char point_buf[BUF_SZ];

    int round_winner = -1;
    int current_max = -1;
    int player_points[N];

    for (int i = 0; i < N; i++) {
        player_points[i] = 0;
    }

    int current_thrown_card[N];
    int winners_count = 0;

    for (int j = 0; j < M; j++) {
        printf("NEW ROUND\n");
        for (int i = 0; i < N; i++) {
            if ((status = write(player_fds[i], write_buf, BUF_SZ)) != BUF_SZ)
                ERR("write() failed");
        }

        for (int i = 0; i < N; i++) {
            if ((status = read(read_fds[i], read_buf, BUF_SZ)) != BUF_SZ)
                ERR("read() failed");
            random_number = atoi(read_buf);
            printf("Got number %d from player %d\n", random_number, i);

            if (random_number > current_max) {
                current_max = random_number;
                round_winner = i;
            }
            current_thrown_card[i] = random_number;
        }

        for (int i = 0; i < N; i++) {
            if (current_thrown_card[i] == current_max) {
                winners_count++;
            }
        }

        for (int i = 0; i < N; i++) {
            if (current_thrown_card[i] == current_max) {
                player_points[i] += N / winners_count;
                memset(point_buf, '0', BUF_SZ);
                sprintf(point_buf, "%d", N / winners_count);
            }
            else {
                memset(point_buf, '0', BUF_SZ);
                sprintf(point_buf, "%d", 0);
            }

            if ((status = write(player_fds[i], point_buf, BUF_SZ)) != BUF_SZ) {
                if (status > 0)
                    printf("Player %d left early...\n", i);
                else
                    ERR("write() failed");
            }
        }

        //printf("ROUND WON BY: %d, chosen card: %d\n", round_winner, current_max);
        //player_points[round_winner] += N;
        winners_count = 0;
        current_max = -1;
        round_winner = -1;
    }

    printf("\nEND OF GAME\n");
    for (int i = 0; i < N; i++) {
        printf("PLAYER %d: %d POINTS\n", i, player_points[i]);
    }
}

void create_players_and_pipes(int N, int M, int* player_fds, int* server_fds) {
    int temp_player_fd[2];
    int temp_server_fd[2];

    for (int i = 0; i < N; i++) {
        if (pipe(temp_player_fd) == -1)
            ERR("pipe() failed");
        player_fds[i] = temp_player_fd[1];

        if (pipe(temp_server_fd) == -1)
            ERR("pipe() failed");
        server_fds[i] = temp_server_fd[0];

        switch (fork()) {
            case -1:
                ERR("fork() failed");
            case 0:
                for (int j = 0; j <= i; j++) {
                    close(player_fds[j]);
                    close(server_fds[j]);
                }

                player_work(M, temp_player_fd[0], temp_server_fd[1], i);
                close(temp_player_fd[0]);
                close(temp_server_fd[1]);

                free(server_fds);
                free(player_fds);
                exit(EXIT_SUCCESS);
            default:
                break;
        }

        close(temp_player_fd[0]);
        close(temp_server_fd[1]);
    }
}

int main(int argc, char** argv) {
    int N, M;
    parse_arguments(argc, argv, &N, &M);
    // printf("N = %d, M = %d\n", N, M);
    int* server_fds = malloc(sizeof(int) * N);
    int* player_fds = malloc(sizeof(int) * N);

    if (sethandler(SIG_IGN, SIGPIPE) == -1)
        ERR("sethandler() failed");

    if (player_fds == NULL)
        ERR("malloc() failed");
    if (server_fds == NULL)
        ERR("malloc() failed");

    create_players_and_pipes(N, M, player_fds, server_fds);
    server_work(N, M, player_fds, server_fds);

    for (int i = 0; i < N; i++) {
        if (close(player_fds[i]) == -1)
            ERR("close() failed");
        if (close(server_fds[i]) == -1)
            ERR("close() failed");
    }

    free(server_fds);
    free(player_fds);

    while (wait(NULL) > 0){}

    return EXIT_SUCCESS;
}