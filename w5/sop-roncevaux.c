#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_KNIGHT_NAME_LENGTH 30

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct knight
{
    char name[MAX_KNIGHT_NAME_LENGTH];
    int hp;
    int atk;
    int fd[2];
} knight;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
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

int parse(knight** out_knights, FILE* f)
{
    int n;
    fscanf(f, "%d", &n);
    knight* knights = malloc(sizeof(knight) * n);
    if (knights)
    {
    }
    for (int i = 0; i < n; i++)
    {
        fscanf(f, "%30s %d %d", knights[i].name, &knights[i].hp, &knights[i].atk);
    }
    *out_knights = knights;
    return n;
}

void knight_work(int k_num, knight k, char* nat, knight* enemy_knights, int enemy_count, knight* knights, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (k_num != i)
            close(knights[i].fd[0]);
        close(knights[i].fd[1]);
    }
    int enemy_fd[enemy_count];
    for (int i = 0; i < enemy_count; i++)
    {
        enemy_fd[i] = enemy_knights[i].fd[1];
        close(enemy_knights[i].fd[0]);
    }
    // printf("Opened descriptors: %d\n", count_descriptors());
    printf("I am %s knight %s. I will serve my king with my %d HP and %d attack.\n", nat, k.name, k.hp, k.atk);

    for (int i = 0; i < enemy_count; i++)
    {
        close(enemy_fd[i]);
    }
    close(knights[k_num].fd[0]);
    printf("Opened descriptors: %d\n", count_descriptors());
    return;
}

void create_knights(int franci_count, int saraceni_count, knight* franci_knights, knight* saraceni_knights)
{
    for (int i = 0; i < franci_count; i++)
    {
        int ret = fork();
        if (ret == -1)
        {
        }
        else if (ret == 0)
        {
        }
        else if (ret > 0)
        {
            knight_work(i, franci_knights[i], "Frankish", saraceni_knights, saraceni_count, franci_knights,
                        franci_count);
            free(franci_knights);
            free(saraceni_knights);
            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < saraceni_count; i++)
    {
        int ret = fork();
        if (ret == -1)
        {
        }
        else if (ret == 0)
        {
        }
        else if (ret > 0)
        {
            knight_work(i, saraceni_knights[i], "Spanish", franci_knights, franci_count, saraceni_knights,
                        saraceni_count);
            free(franci_knights);
            free(saraceni_knights);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());

    FILE *franci, *saraceni;
    franci = fopen("franci.txt", "r");
    if (franci == NULL)
    {
        fprintf(stderr, "Franks have not arrived on the battlefield.");
        return EXIT_FAILURE;
    }

    saraceni = fopen("saraceni.txt", "r");
    if (saraceni == NULL)
    {
        fprintf(stderr, "Saracens have not arrived on the battlefield.");
        return EXIT_FAILURE;
    }

    knight* franci_knights;
    int franci_n = parse(&franci_knights, franci);
    knight* saraceni_knights;
    int saraceni_n = parse(&saraceni_knights, saraceni);

    fclose(franci);
    fclose(saraceni);

    for (int i = 0; i < franci_n; i++)
    {
        pipe(franci_knights[i].fd);
    }
    for (int i = 0; i < saraceni_n; i++)
    {
        pipe(saraceni_knights[i].fd);
    }

    create_knights(franci_n, saraceni_n, franci_knights, saraceni_knights);

    while (wait(NULL) > 0)
    {
    }

    free(franci_knights);
    free(saraceni_knights);

    return EXIT_SUCCESS;
}
