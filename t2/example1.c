#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define MAX_NAME 1001

volatile sig_atomic_t last_signal = 0;
volatile sig_atomic_t sig_count = 0;

void sigalrm_handler(int sig)
{
    last_signal = sig;
}

void sigusr_handler(int sig)
{
    sig_count++;
}

void sigchld_handler(int sig)
{
    pid_t pid;

    while (1)
    {
        pid = waitpid(0, NULL, WNOHANG);

        if (pid == 0)
            return;

        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
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

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;

    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;

        buf += c;
        len += c;
        count -= c;
    } while (count > 0);

    return len;
}

void child_work(char* num, sigset_t mask)
{
    int count = 0;
    srand(getpid());
    int s = 10 + rand() % 91;
    s = s * 1024;

    char name[MAX_NAME];
    sprintf(name, "%d", getpid());
    strcat(name, ".TXT");

    char* buf = malloc(s + 1);
    if (!buf)
        ERR("malloc");
    memset(buf, num[0], s);
    buf[s] = '\0';
    int fd;
    if ((fd = open(name, O_WRONLY | O_TRUNC | O_CREAT | O_APPEND, 0777)) < 0)
        ERR("open");

    alarm(1);
    while (last_signal != SIGALRM)
    {
        sigsuspend(&mask);
        while (sig_count != count)
        {
            bulk_write(fd, buf, s);
            count++;
        }
    }
    while (sig_count > count)
    {
        bulk_write(fd, buf, s);
        count++;
    }

    if (close(fd))
        ERR("close");
    free(buf);
    exit(EXIT_SUCCESS);
}

void parent_work()
{
    struct timespec t = {0, 10000000};
    alarm(1);

    while (last_signal != SIGALRM)
    {
        kill(0, SIGUSR1);
        nanosleep(&t, NULL);
    }
}

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s n m ...\n", name);
    fprintf(stderr, "n, m, ... - numbers [0 - 9]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc < 2)
        usage(argv[0]);
    for (int i = 1; i < argc; i++)
    {
        if (atoi(argv[i]) > 9 || atoi(argv[i]) < 0)
        {
            usage(argv[0]);
        }
    }

    pid_t pid;
    sethandler(SIG_IGN, SIGUSR1);
    sethandler(sigchld_handler, SIGCHLD);
    sethandler(sigalrm_handler, SIGALRM);
    sigset_t mask, omask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &omask);

    for (int i = 1; i < argc; i++)
    {
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            sethandler(sigusr_handler, SIGUSR1);
            child_work(argv[i], omask);
        }
    }

    parent_work();
    while (wait(NULL) > 0)
    {
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    printf("[PARENT] terminates\n");
    return EXIT_SUCCESS;
}
