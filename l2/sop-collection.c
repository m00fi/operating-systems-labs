#include <bits/types/sigset_t.h>
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define ITER_COUNT 25
#define MAX_NAME 1001
#define MAX_BUF 4096

volatile sig_atomic_t sig_num = -1;
volatile sig_atomic_t usr1_ = 0;

void usr1_handle(int sig)
{
    sig_num = 0;
    usr1_ = 1;
}
void usr2_handle(int sig) { sig_num = 1; }
void pipe_handle(int sig) { sig_num = 2; }
void int_handle(int sig) { sig_num = 3; }

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
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

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&ts, &ts)))
        ERR("nanosleep");
}

void darczynca_work(sigset_t omask, int n, pid_t skarbona_pid)
{
    sigsuspend(&omask);
    printf("[%d] Skierowany do skarbony nr %d\n", getpid(), sig_num);
    srand(getpid());
    int money = 100 + rand() % 1901;

    if (sig_num + 1 > n)
    {
        printf("[%d] Nic tu nie ma, wracam do domu!\n", getpid());
        return;
    }
    else
    {
        char name[MAX_NAME];
        char buf[MAX_BUF];
        snprintf(name, MAX_NAME, "skarbona_%d", skarbona_pid);
        int fd = open(name, O_WRONLY | O_APPEND);
        if (fd < 0)
            ERR("open");

        snprintf(buf, 10, "%x %x\n", getpid(), money);
        if (TEMP_FAILURE_RETRY(bulk_write(fd, buf, 10)) < 0)
            ERR("bulk_write");

        printf("[%d] Wrzucam %d zł\n", getpid(), money);
        if (kill(skarbona_pid, SIGUSR1) < 0)
            ERR("kill");
        TEMP_FAILURE_RETRY(close(fd));
    }
}

void skarbona_work(sigset_t omask)
{
    printf("[%d] Skarbona otwarta\n", getpid());
    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "skarbona_%d", getpid());
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        ERR("open");
    char buf1[4];
    char buf2[4];
    sigprocmask(SIG_UNBLOCK, &omask, NULL);

    while (1)
    {
        if (usr1_ == 1)
        {
            lseek(fd, 0, SEEK_END);
            bulk_read(fd, buf1, 4);
            lseek(fd, 5, SEEK_CUR);
            bulk_read(fd, buf2, 4);
            int id = atoi(buf1);
            int money = atoi(buf2);
            printf("[%d] Obywatel %d wrzucil %d zł", getpid(), id, money);
            usr1_ = 0;
        }
    }
    close(fd);
}

void usage(int argc, char* argv[])
{
    printf("%s n\n", argv[0]);
    printf("\t1 <= n <= 4 -- number of moneyboxes\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
        usage(argc, argv);

    int n = atoi(argv[1]);
    if (n < 1 || n > 4)
        usage(argc, argv);

    pid_t children[4];
    pid_t pid;

    sigset_t mask, omask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &omask);

    for (int i = 0; i < n; i++)
    {
        switch (pid = fork())
        {
            case 0:
                sethandler(usr1_handle, SIGUSR1);
                skarbona_work(mask);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                children[i] = pid;
                break;
        }
    }

    srand(getpid());
    int r;
    int sigtab[4] = {SIGUSR1, SIGUSR2, SIGPIPE, SIGINT};

    for (int i = 0; i < ITER_COUNT; i++)
    {
        r = rand() % 4;
        switch (pid = fork())
        {
            case 0:
                sethandler(usr1_handle, SIGUSR1);
                sethandler(usr2_handle, SIGUSR2);
                sethandler(pipe_handle, SIGPIPE);
                sethandler(int_handle, SIGINT);
                darczynca_work(omask, n, children[r]);

                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                ms_sleep(100);
                if (kill(pid, sigtab[r]))
                    ERR("kill");
                waitpid(pid, NULL, WUNTRACED);
                break;
        }
    }

    // kill(0, SIGTERM);

    while (wait(NULL) > 0)
    {
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("[K] Zbiórka zakończona!\n");
    return EXIT_SUCCESS;
}
