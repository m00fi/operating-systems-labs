#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BSIZE 500

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
    fprintf(stderr, "N - number of players\n");
    fprintf(stderr, "M - start amount\n");
    exit(EXIT_FAILURE);
}
void dealer_work(int* player_fds, int* dealer_fds, pid_t* player_pids, int N) {
    srand(getpid());

    int* bet_number = malloc(sizeof(int) * N);
    int* player_bet = malloc(sizeof(int) * N);
    char buffer[BSIZE];
    ssize_t status;

    int* is_active = malloc(sizeof(int) * N);

    while (1) {
        int players_left = 0;

        for (int i = 0; i < N; i++) {
            memset(buffer, '0', BSIZE);
            if ((status = read(dealer_fds[i], buffer, BSIZE)) != BSIZE) {
                if (status == 0) {
                    is_active[i] = -1;
                    continue;
                }

                ERR("read() failed");
            }
            players_left++;
            char* _bet = strtok(buffer, "_");
            char* _number = strtok(NULL, "_");
            player_bet[i] = atoi(_bet);
            int number = atoi(_number);
            bet_number[i] = number;
            printf("Dealer: %d placed %d on %d.\n", player_pids[i], player_bet[i], number);
        }
        if (players_left == 0)
            break;

        int winner_number = rand() % 37;
        printf("Dealer: %d is lucky number.\n", winner_number);

        for (int i = 0; i < N; i++) {
            if (is_active[i] == -1)
                continue;

            memset(buffer, '0', BSIZE);
            if (bet_number[i] == winner_number) {
                int won_money = player_bet[i] * 35;

                sprintf(buffer, "%d", won_money);
                if ((status = write(player_fds[i], buffer, BSIZE)) != BSIZE) {
                    if (status == 0)
                        continue;
                    ERR("write() failed");
                }
            }
            else {
                sprintf(buffer, "%d", 0);
                if ((status = write(player_fds[i], buffer, BSIZE)) != BSIZE){
                    if (status == 0)
                        continue;
                    ERR("write() failed");
                }
            }
        }
    }

    free(is_active);
    free(player_bet);
    free(bet_number);
}

void player_work(int start_money, int read_fd, int write_fd) {
    srand(getpid());
    printf("%d: I have %d and I'm going to play roulette.\n", getpid(), start_money);

    char buffer[BSIZE];
    ssize_t status;
    int current_money = start_money;

    while (current_money > 0) {
        if (rand() % 9 == 0) {
            break;
        }

        memset(buffer, '0', BSIZE);
        int bet = 1 + rand() % (current_money);
        current_money = current_money - bet;
        int number = rand() % 37;

        sprintf(buffer, "%d_%d", bet, number);
        if ((status = write(write_fd, buffer, BSIZE)) != BSIZE)
            ERR("write() failed");

        if ((status = read(read_fd, buffer, BSIZE)) != BSIZE)
            ERR("read() failed");

        int won = atoi(buffer);
        if (won > 0) {
            printf("%d: HOLY SHIT I WON %d!\n", getpid(), won);
        }

        current_money = current_money + won;
    }
    if (current_money > 0) {
        printf("%d: I SAVED %d!\n", getpid(), current_money);
    }
    else
        printf("%d: I'm broke...\n", getpid());
}

void create_players(int N, int M, int* player_fds, int* dealer_fds, pid_t* player_pids) {
    int temp_player_fd[2];
    int temp_dealer_fd[2];


    for (int i = 0; i < N; i++) {
        if (pipe(temp_player_fd) == -1)
            ERR("pipe() failed");
        if (pipe(temp_dealer_fd) == -1)
            ERR("pipe() failed");
        player_fds[i] = temp_player_fd[1];
        dealer_fds[i] = temp_dealer_fd[0];

        switch (player_pids[i] = fork()) {
            case -1:
                ERR("fork() failed");
            case 0:
                for (int j = 0; j < i; j++) {
                    close(player_fds[j]);
                    close(dealer_fds[j]);
                }
                free(player_pids);
                free(dealer_fds);
                free(player_fds);
                close(temp_player_fd[1]);
                close(temp_dealer_fd[0]);

                player_work(M, temp_player_fd[0], temp_dealer_fd[1]);
                close(temp_player_fd[0]);
                close(temp_dealer_fd[1]);

                exit(EXIT_SUCCESS);
            default:
                break;
        }
        close(temp_player_fd[0]);
        close(temp_dealer_fd[1]);
    }
}

void parse_args(int argc, char** argv, int* N, int* M) {
    if (argc != 3)
        usage(argv[0]);

    *N = atoi(argv[1]);
    *M = atoi(argv[2]);

    if (*N < 1 || *M < 100)
        usage(argv[0]);
}

int main(int argc, char** argv) {
    int N, M;
    parse_args(argc, argv, &N, &M);

    if (sethandler(SIG_IGN, SIGPIPE) == -1)
        ERR("sethandler() failed");

    int* player_fds = malloc(sizeof(int) * N);
    int* dealer_fds = malloc(sizeof(int) * N);
    pid_t* player_pids = malloc(sizeof(pid_t) * N);
    if (!player_fds || !dealer_fds || !player_pids)
        ERR("malloc() failed");

    create_players(N, M, player_fds, dealer_fds, player_pids);
    dealer_work(player_fds, dealer_fds, player_pids, N);

    free(player_fds);
    free(dealer_fds);
    free(player_pids);
    while (wait(NULL) > 0) {}

    printf("Dealer: Casino always wins.\n");
    return EXIT_SUCCESS;
}