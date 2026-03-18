#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_BUF 200

#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sig_handle(int sig) { last_signal = sig; }

void kill_me(int sig) {
    if (rand() % 5 == 0)
        exit(EXIT_SUCCESS);
}

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid)
            return;
        if (0 >= pid)
        {
            if (ECHILD == errno)
                return;
            ERR("waitpid:");
        }
    }
}

void usage(char* name) {
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "n - number of child processes\n");
    exit(EXIT_FAILURE);
}

void parent_work(int n, int *fds, int R) {
    unsigned char c;
    char buf[MAX_BUF];
    int status, i;
    srand(getpid());
    if (sethandler(sig_handle, SIGINT))
        ERR("Setting SIGINT handler in parent");
    for (;;)
    {
        if (SIGINT == last_signal)
        {
            i = rand() % n;
            while (0 == fds[i % n] && i < 2 * n)
                i++;
            i %= n;
            if (fds[i])
            {
                c = 'a' + rand() % ('z' - 'a');
                status = TEMP_FAILURE_RETRY(write(fds[i], &c, 1));
                if (status != 1)
                {
                    if (TEMP_FAILURE_RETRY(close(fds[i])))
                        ERR("close");
                    fds[i] = 0;
                }
            }
            last_signal = 0;
        }
        status = read(R, &c, 1);
        if (status < 0 && errno == EINTR)
            continue;
        if (status < 0)
            ERR("read header from R");
        if (0 == status)
            break;
        if (TEMP_FAILURE_RETRY(read(R, buf, c)) < c)
            ERR("read data from R");
        buf[(int)c] = 0;
        printf("\n%s\n", buf);
    }
}

void child_work(int fd, int R) {
    srand(getpid());
    char c, buf[MAX_BUF + 1];
    unsigned char s;
    if (sethandler(kill_me, SIGINT))
        ERR("sethandler");
    //
    // if (write(R, &c, 1) < 0)
    //     ERR("write");
    for (;;) {
        if (TEMP_FAILURE_RETRY(read(fd, &c, 1)) < 1)
            ERR("read");
        s = 1 + rand() % MAX_BUF;
        buf[0] = s;
        memset(buf + 1, c, s);
        if (TEMP_FAILURE_RETRY(write(R, buf, s + 1)) < 0)
            ERR("write R");
    }
}

void create_children_and_pipes(int n, int* fds, int R) {
    int tempfd[2];
    int max = n;

    while (n)
    {
        if (pipe(tempfd) == -1)
            ERR("pipe");

        switch (fork())
        {
            case -1:
                ERR("fork");
            case 0:
                while (n < max)
                    if (fds[n] && close(fds[n++]))
                        ERR("close");
                free(fds);
                if (close(tempfd[1]))
                    ERR("close");
                child_work(tempfd[0], R);
                if (close(tempfd[0]))
                    ERR("close");
                if (close(R))
                    ERR("close");
                exit(EXIT_SUCCESS);
            default:
                break;
        }
        if (close(tempfd[0]))
            ERR("close");
        fds[--n] = tempfd[1];
    }
}


int main(int argc, char **argv) {
    if (argc != 2)
        usage(argv[0]);

    int child_num = atoi(argv[1]);
    int* fds = malloc(sizeof(int) * child_num);
    int pipe_R[2];
    if (pipe(pipe_R) == -1)
        ERR("pipe");
    if (sethandler(sigchld_handler, SIGCHLD))
        ERR("Setting parent SIGCHLD:");
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Setting SIGPIPE:");
    if (sethandler(SIG_IGN, SIGINT))
        ERR("Setting SIGINT:");

    // sigset_t set, oset;
    // sigemptyset(&set);
    // sigaddset(&set, SIGINT);
    // sigaddset(&set, SIGPIPE);
    // sigprocmask(SIG_BLOCK, &set, &oset);

    create_children_and_pipes(child_num, fds, pipe_R[1]);
    if (close(pipe_R[1]))
        ERR("close");
    parent_work(child_num, fds, pipe_R[0]);

    while (child_num--)
        if (fds[child_num] && close(fds[child_num]))
            ERR("close");
    if (close(pipe_R[0]))
        ERR("close");
    free(fds);
    return EXIT_SUCCESS;
}