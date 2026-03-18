#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;
volatile sig_atomic_t cough_count = 0;
volatile sig_atomic_t is_ill = 0;
volatile sig_atomic_t probability = 0;
void sig_handle(int sig) { last_signal = sig; }

void sigusr_handle(int sig, siginfo_t* siginfo, void* x)
{
    pid_t pid = siginfo->si_pid;
    printf("Child[%d]: %d has coughed at me!\n", getpid(), pid);
    if (rand() % 101 <= probability)
    {
        printf("Child[%d] got sick!\n", getpid());
        is_ill = 1;
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

void setusrhandler(void (*f)(int, siginfo_t*, void*), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void child_work(sigset_t mask)
{
    srand(getpid());

    printf("Child[%d] starts day in the kindergarten, ill: %d\n", getpid(), is_ill);
    while (last_signal != SIGTERM)
    {
        if (is_ill == 1)
        {
            sethandler(SIG_IGN, SIGUSR1);
            struct timespec time = {0, 50000000 + rand() % 151000000};
            nanosleep(&time, NULL);
            cough_count++;
            printf("Child[%d] is coughing %d\n", getpid(), cough_count);
            if (kill(0, SIGUSR1))
                ERR("kill");
        }
        sleep(3);
    }
}

void parent_work(int t)
{
    alarm(t);
    printf("KG[%d]: Alarm has been set for %d sec\n", getpid(), t);
    while (last_signal != SIGALRM)
    {
    }
    printf("KG[%d]: Simulation has ended!\n", getpid());

    if (kill(0, SIGTERM))
        ERR("kill");
}

void create_children(int n, sigset_t mask, int t)
{
    switch (fork())
    {
        case 0:
            is_ill = 1;
            sigprocmask(SIG_UNBLOCK, &mask, NULL);
            sethandler(sig_handle, SIGTERM);
            setusrhandler(sigusr_handle, SIGUSR1);
            child_work(mask);
            printf("Child[%d] exits with %d\n", getpid(), EXIT_SUCCESS);
            exit(EXIT_SUCCESS);
        case -1:
            ERR("fork");
        default:
            break;
    }

    for (int i = 1; i < n; i++)
    {
        switch (fork())
        {
            case 0:
                sigprocmask(SIG_UNBLOCK, &mask, NULL);
                sethandler(sig_handle, SIGTERM);
                setusrhandler(sigusr_handle, SIGUSR1);
                child_work(mask);
                printf("Child[%d] exits with %d\n", getpid(), EXIT_SUCCESS);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                break;
        }
    }

    parent_work(t);
}

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s t k n p\n", name);
    fprintf(stderr, "t - simulation time (in seconds) (1 - 100)\n");
    fprintf(stderr, "k - time till ill child pick-up (in seconds) (1 - 100)\n");
    fprintf(stderr, "n - number of children (1 - 30)\n");
    fprintf(stderr, "p - probability of getting ill (procentage) (1 - 100)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc != 5)
        usage(argv[0]);
    int t, k, n;
    t = atoi(argv[1]);
    k = atoi(argv[2]);
    n = atoi(argv[3]);
    probability = atoi(argv[4]);

    if (t < 1 || t > 100 || k < 1 || k > 100 || n < 1 || n > 30 || probability < 1 || probability > 100)
        usage(argv[0]);

    sigset_t mask, omask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    sethandler(sig_handle, SIGTERM);
    sethandler(SIG_IGN, SIGUSR1);
    sethandler(sig_handle, SIGALRM);

    create_children(n, mask, t);

    while (wait(NULL) > 0)
    {
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("[PARENT] terminates\n");
    return EXIT_SUCCESS;
}
