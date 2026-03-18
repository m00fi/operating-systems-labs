#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (sigaction(sigNo, &act, NULL) == -1)
        ERR("sigaction");
}

void sig_handler(int sig)
{
    last_signal = sig;
}

void sigchld_handler(int sig)
{
    (void)sig;
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

void parent_work(sigset_t mask)
{
    int count = 0;
    while (1)
    {
        last_signal = 0;
        while (last_signal != SIGUSR2)
        {
            sigsuspend(&mask);
        }
        count++;
        printf("[PARENT] received SIGUSR2, count: %d\n", count);
    }
}

void child_work(int k, int t)
{
    pid_t ppid = getppid();
    int counter = 0;
    struct timespec time = {0, k * 10000};
    while (1)
    {
        for (int i = 0; i < t; i++)
        {
            nanosleep(&time, NULL);
            if (kill(ppid, SIGUSR1) == -1)
                ERR("kill");
        }
        nanosleep(&time, NULL);
        if (kill(ppid, SIGUSR2) == -1)
            ERR("kill");
        counter++;
        printf("[%d] no. of SIGUSR2 sent: %d\n", getpid(), counter);
    }

    printf("[%d] terminates\n", getpid());
}

void usage(void)
{
    fprintf(stderr, "USAGE: signals k t\n");
    fprintf(stderr, "k - time between signals (microseconds)\n");
    fprintf(stderr, "t - 1 in t times send SIGUSR2 instead of SIGUSR1\n");

    exit(EXIT_FAILURE);
}

int main(const int argc, char** argv)
{
    if (argc != 3)
        usage();

    const int k = atoi(argv[1]);
    const int t = atoi(argv[2]);
    if (k < 1 || t < 1)
        usage();

    sigset_t mask;
    sigset_t oldmask;

    sethandler(sigchld_handler, SIGCHLD);
    sethandler(sig_handler, SIGUSR1);
    sethandler(sig_handler, SIGUSR2);

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    pid_t pid;
    if ((pid = fork()) < 0)
        ERR("fork");
    if (pid == 0)
        child_work(k, t);
    else
    {
        parent_work(oldmask);
        while (wait(NULL) > 0);
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    return EXIT_SUCCESS;
}
