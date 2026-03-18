#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_BUF 255

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

void start(int write_fd) {
    int n = 1;
    char buf[MAX_BUF];
    unsigned char s;
    memset(buf, 0, MAX_BUF);
    s = sprintf(buf + 1,"%d", n);
    buf[0] = s;

    if (write(write_fd, buf, s + 1) != s + 1)
        ERR("write() failed");
}

void work(int read_fd, int write_fd, char* name) {
    srand(getpid());
    int status;
    char buf[MAX_BUF];
    unsigned char s;
    memset(buf, 0, MAX_BUF);
    int n;

    while (1) {
        //sleep(1);
        memset(buf, 0, MAX_BUF);
        if ((status = read(read_fd, &s, 1)) < 0)
            ERR("read() failed");
        if (status == 0)
            break;
        if (read(read_fd, buf, s) != s)
            ERR("read() failed");

        n = atoi(buf);
        printf("[%s] [%d] read n: %d\n", name, getpid(), n);

        if (n == 0)
            return;

        memset(buf, 0, MAX_BUF);
        int r_n = rand() % 21 - 10;
        n += r_n;
        s = sprintf(buf + 1,"%d", n);
        buf[0] = s;

        if (write(write_fd, buf, s + 1) != s + 1)
            ERR("write() failed");
    }
}

void create_children_and_pipes() {
    int fds[3][2];

    for (int i = 0; i < 3; i++) {
        if (pipe(fds[i]))
            ERR("pipe() failed");
    }

    switch (fork()) {
        case -1:
            ERR("fork() failed");
        case 0:
            if (close(fds[0][1]))
                ERR("close() failed");
            if (close(fds[1][0]))
                ERR("close() failed");
            if (close(fds[2][0]))
                ERR("close() failed");
            if (close(fds[2][1]))
                ERR("close() failed");

            work(fds[0][0], fds[1][1], "child1");
            if (close(fds[0][0]))
                ERR("close() failed");
            if (close(fds[1][1]))
                ERR("close() failed");
            printf("child 1 exit\n");
            exit(EXIT_SUCCESS);
    }

    switch (fork()) {
        case -1:
            ERR("fork() failed");
        case 0:
            if (close(fds[0][0]))
                ERR("close() failed");
            if (close(fds[0][1]))
                ERR("close() failed");
            if (close(fds[1][1]))
                ERR("close() failed");
            if (close(fds[2][0]))
                ERR("close() failed");

            work(fds[1][0], fds[2][1], "child2");
            if (close(fds[1][0]))
                ERR("close() failed");
            if (close(fds[2][1]))
                ERR("close() failed");
            printf("child 2 exit\n");
            exit(EXIT_SUCCESS);
    }
    if (close(fds[0][0]))
        ERR("close() failed");
    if (close(fds[1][0]))
        ERR("close() failed");
    if (close(fds[1][1]))
        ERR("close() failed");
    if (close(fds[2][1]))
        ERR("close() failed");

    start(fds[0][1]);
    work(fds[2][0], fds[0][1], "parent");
    if (close(fds[2][0]))
        ERR("close() failed");
    if (close(fds[0][1]))
        ERR("close() failed");
}

int main() {
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("sethandler() failed");

    create_children_and_pipes();
    while (wait(NULL) > 0){}
    return EXIT_SUCCESS;
}