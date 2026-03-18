#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define FILE_MAX_SIZE 512

volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t sig_usr2 = 0;
volatile sig_atomic_t sig_int = 0;

void usr1_handler(int sig) { sig_usr1 = 1; }
void usr2_handler(int sig) { sig_usr2 = 1; }
void int_handler(int sig) { sig_int = 1; }

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
            return len; // EOF
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

void create_and_read(char* name, int is_root, sigset_t omask)
{
    printf("My name is %s and my PID is %d\n", name, getpid());
    int fd;
    if ((fd = open(name, O_RDONLY)) < 0)
        ERR("open");
    char buf[FILE_MAX_SIZE];
    ssize_t size = bulk_read(fd, buf, FILE_MAX_SIZE);
    if (size == -1)
        ERR("bulk_read");
    close(fd);
    buf[size] = '\0';
    char* child_name = strtok(buf, "\n");
    pid_t children[2] = {0};
    int children_count = 0;
    pid_t pid;

    while (child_name)
    {
        if (strcmp(child_name, "-") == 0)
        {
            child_name = strtok(NULL, "\n");
            continue;
        }
        printf("%s is inspecting %s.\n", name, child_name);
        switch (pid = fork())
        {
            case 0:
                create_and_read(child_name, 0, omask);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
            default:
                children[children_count++] = pid;
                child_name = strtok(NULL, "\n");
        }
    }
    printf("%s has inspected all subordinates.\n", name);

    srand(getpid());
    while (sigsuspend(&omask))
    {
        if (sig_usr2)
        {
            sig_usr2 = 0;
            if (is_root)
                printf("%s received a document. Ignoring...\n", name);
            else
            {
                switch (rand() % 3)
                {
                    case 0:
                        printf("%s received a document. Sending to the archive.\n", name);

                        break;
                    case 1:
                    case 2:
                        printf("%s received a document. Passing on to the superintendent.\n", name);
                        if (kill(getppid(), SIGUSR2))
                            ERR("kill");
                        break;
                }
            }
        }

        if (sig_int)
        {
            printf("%s is ending the day.\n", name);
            if (children[0] > 0)
                kill(children[0], SIGINT);
            if (children[1] > 0)
                kill(children[1], SIGINT);

            break;
        }
    }
    while (wait(NULL) > 0)
    {
    }
    printf("%s is leaving the office.\n", name);
}

void usage(int argc, char* argv[])
{
    printf("%s p h\n", argv[0]);
    printf("\tp - path to directory describing the structure of the Austro-Hungarian office in Prague.\n");
    printf("\th - Name of the highest administrator.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);
    if (chdir(argv[1]))
        ERR("chdir");

    sigset_t mask, omask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    sethandler(usr1_handler, SIGUSR1);
    sethandler(usr2_handler, SIGUSR2);
    sethandler(int_handler, SIGINT);

    printf("Waiting for SIGUSR1...\n");
    while (!sig_usr1 && sigsuspend(&omask))
    {
    }
    printf("Got SIGUSR1 !\n");

    create_and_read(argv[2], 1, omask);

    return EXIT_SUCCESS;
}
