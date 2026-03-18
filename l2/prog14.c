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
    printf("[%d] received signal %d\n", getpid(), sig);
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

void parent_work(int k, int p, int r)
{
    struct timespec tk = {k, 0};
    struct timespec tp = {p, 0};

    sethandler(sig_handler, SIGALRM);
    alarm(r * 10);
    while (last_signal != SIGALRM)
    {
        nanosleep(&tk, NULL);
        if (kill(0, SIGUSR1) < 0)
            ERR("kill");

        nanosleep(&tp, NULL);
        if (kill(0, SIGUSR2) < 0)
            ERR("kill");
    }
    printf("[PARENT] Terminates \n");
}

void child_work(int r)
{
    int tt, t;
    srand(getpid());
    t = 5 + rand() % 6;
    for (int i = 0; i < r; i++)
    {
        for (tt = t; tt > 0; tt = sleep(tt))
        {
        }
        if (last_signal == SIGUSR1)
            printf("[%d] SUCCESS\n", getpid());
        else
            printf("[%d] FAILURE\n", getpid());
    }
    printf("[%d] terminates\n", getpid());
}

void create_children(int n, int r)
{
    for (int i = 0; i < n; ++i)
    {
        switch (fork())
        {
            case -1:
                ERR("fork");
            case 0:
                sethandler(sig_handler, SIGUSR1);
                sethandler(sig_handler, SIGUSR2);
                child_work(r);
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
}

void usage(void)
{
    fprintf(stderr, "USAGE: signals n k p l\n");
    fprintf(stderr, "n - number of children\n");
    fprintf(stderr, "k - Interval before SIGUSR1\n");
    fprintf(stderr, "p - Interval before SIGUSR2\n");
    fprintf(stderr, "l - lifetime of child in cycles\n");

    exit(EXIT_FAILURE);
}

int main(const int argc, char** argv)
{
    if (argc != 5)
        usage();

    int n = atoi(argv[1]);
    const int k = atoi(argv[2]);
    const int p = atoi(argv[3]);
    const int r = atoi(argv[4]);
    if (n < 1 || k < 1 || p < 1 || r < 1)
        usage();

    sethandler(sigchld_handler, SIGCHLD);
    sethandler(SIG_IGN, SIGUSR1);
    sethandler(SIG_IGN, SIGUSR2);

    create_children(n, r);
    parent_work(k, p, r);

    while (wait(NULL) > 0)
    {
    }

    return EXIT_SUCCESS;
}
