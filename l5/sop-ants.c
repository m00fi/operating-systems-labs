// #define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_GRAPH_NODES 32
#define MAX_PATH_LENGTH (2 * MAX_GRAPH_NODES)

#define BSIZE (sizeof(int) + MAX_PATH_LENGTH * sizeof(int) + sizeof(int))

#define FIFO_NAME "/tmp/colony_fifo"

volatile sig_atomic_t run = 1;

typedef struct Ant
{
    int id;
    int path[MAX_PATH_LENGTH];
    int path_length;
} ant_t;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void sigint_handle(int sigNo) { run = 0; }

void msleep(int ms)
{
    struct timespec tt;
    tt.tv_sec = ms / 1000;
    tt.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void usage(int argc, char* argv[])
{
    printf("%s graph start dest\n", argv[0]);
    printf("  graph - path to file containing colony graph\n");
    printf("  start - starting node index\n");
    printf("  dest - destination node index\n");
    exit(EXIT_FAILURE);
}

void parse_file(FILE* f, int vert[MAX_GRAPH_NODES][MAX_GRAPH_NODES], int* n)
{
    fscanf(f, "%d", n);

    int from, to;

    while (fscanf(f, "%d %d", &from, &to) != EOF)
    {
        vert[from][to] = 1;
    }
}

void child_work(int idx, int vert[MAX_GRAPH_NODES][MAX_GRAPH_NODES], int n, int read_fd, int end_idx)
{
    srand(getpid());
    if (set_handler(sigint_handle, SIGINT) == -1)
        ERR("set_handler");

    printf("%d: ", idx);
    for (int i = 0; i < n; i++)
    {
        if (vert[idx][i] != -1)
            printf("%d ", i);
    }
    printf("\n");
    ssize_t status;
    char buffer[BSIZE];

    for (;;)
    {
        if ((status = read(read_fd, buffer, BSIZE)) != BSIZE)
        {
            if (run == 0)
                break;
            else if (status == 0 && errno == EPIPE)
                break;
            else
                ERR("read");
        }
        if (run == 0)
            break;

        ant_t a;
        memcpy(&a, buffer, BSIZE);
        // printf("ant id: %d\n", a.id);
        a.path[a.path_length] = idx;
        a.path_length++;
        printf("vert %d: ant %d entered\n", idx, a.id);

        if (idx == end_idx)
        {
            printf("Ant %d: FOUND FOOD\n", a.id);
        }

        if (a.path_length >= MAX_PATH_LENGTH)
            printf("And %d: got lost\n", a.id);
        else
        {
            int next;
            do
            {
                next = rand() % n;
            } while (vert[idx][next] == -1);
            memcpy(buffer, &a, BSIZE);
            if ((status = write(vert[idx][next], buffer, BSIZE)) != BSIZE)
            {
                if (run == 0)
                    break;
                else if (status == 0 && errno == EPIPE)
                    break;
                else
                    ERR("read");
            }
        }

        msleep(100);
    }
    // printf("open dsc: %d\n", count_descriptors());
}

int create_children(int start_idx, int end_idx, int n, int vert[MAX_GRAPH_NODES][MAX_GRAPH_NODES])
{
    int vert_fd[MAX_GRAPH_NODES][2];
    for (int i = 0; i < n; i++)
    {
        if (pipe(vert_fd[i]) == -1)
            ERR("pipe");
    }

    for (int i = 0; i < n; i++)
    {
        switch (fork())
        {
            case -1:
                ERR("fork");
            case 0:

                for (int v = 0; v < n; v++)
                {
                    if (v != i)
                        close(vert_fd[v][0]);

                    if (vert[i][v] != -1)
                        vert[i][v] = vert_fd[v][1];
                    else
                        close(vert_fd[v][1]);
                }
                child_work(i, vert, n, vert_fd[i][0], end_idx);

                close(vert_fd[i][0]);
                for (int v = 0; v < n; v++)
                {
                    if (vert[i][v] != -1)
                        close(vert[i][v]);
                }

                // printf("open dsc: %d\n", count_descriptors());
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }

    for (int i = 0; i < n; i++)
    {
        if (i != start_idx)
        {
            close(vert_fd[i][0]);
            close(vert_fd[i][1]);
        }
    }
    close(vert_fd[start_idx][0]);

    count_descriptors();

    return vert_fd[start_idx][1];
}

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argc, argv);
    int vert[MAX_GRAPH_NODES][MAX_GRAPH_NODES];
    for (int i = 0; i < MAX_GRAPH_NODES; i++)
    {
        for (int j = 0; j < MAX_GRAPH_NODES; j++)
        {
            vert[i][j] = -1;
        }
    }

    if (set_handler(SIG_IGN, SIGPIPE) == -1)
        ERR("set_handler");
    if (set_handler(SIG_IGN, SIGINT) == -1)
        ERR("set_handler");

    int end_idx = atoi(argv[3]);
    int start_idx = atoi(argv[2]);
    char* colony_filename = argv[1];
    FILE* colony;
    colony = fopen(colony_filename, "r");
    int n;

    parse_file(colony, vert, &n);
    fclose(colony);

    int start_fd = create_children(start_idx, end_idx, n, vert);

    if (set_handler(sigint_handle, SIGINT) == -1)
        ERR("set_handler");

    ssize_t status;
    int current_id = 1;
    char buffer[BSIZE];

    for (;;)
    {
        msleep(1000);
        memset(buffer, 0, BSIZE);
        ant_t a;
        a.id = current_id;
        a.path_length = 0;
        memcpy(buffer, &a, BSIZE);

        if ((status = write(start_fd, buffer, BSIZE)) != BSIZE)
        {
            if (status == 0 || errno == EPIPE)
                break;
            else if (run == 0)
                break;
            else
                ERR("write");
        }

        current_id++;
    }

    close(start_fd);

    while (wait(NULL) > 0)
    {
    }
    // printf("open dsc: %d\n", count_descriptors());

    exit(EXIT_SUCCESS);
}
