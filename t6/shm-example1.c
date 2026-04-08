#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define CHAR_COUNT 256

#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void count_data(int len, char* data, int* character, int start_idx, int stop_idx)
{
    int it = 0;
    while (it < len) {
        int idx = (int)data[it];

        if (idx >= start_idx && idx <= stop_idx) {
            *(character + idx * sizeof(int)) += 1;
        }
        it++;
    }
}

void child_work(char* file_name, int start_idx, int stop_idx, int* character) {
    srand(getpid());
    int file_fd;
    if ((file_fd = open(file_name, O_RDONLY)) == -1)
        ERR("open");
    int file_length = lseek(file_fd, 0, SEEK_END) + 1;

    char* data;
    if ((data = (char*)mmap(NULL, file_length, PROT_READ, MAP_SHARED, file_fd, 0)) == MAP_FAILED)
        ERR("mmap");
    close(file_fd);

    count_data(file_length, data, character, start_idx, stop_idx);

    if (rand() % 100 < 5)
        abort();

    if (munmap(data, file_length))
        ERR("munmap");
}

void parent_work(int* character) {
    pid_t pid;
    int status;
    int abort = 0;
    for (;;)
    {
        pid = wait(&status);

        if (pid <= 0)
        {
            if (errno == ECHILD)
                break;
            ERR("waitpid");
        }
        if (WIFSIGNALED(status))
            abort = 1;
    }
    if (abort == 1) {
        printf("CALCULATIONS FAILED\n");
        return;
    }

    printf("\nCOUNTED SYMBOLS:\n");
    for (int i = 0; i < CHAR_COUNT; i++) {
        if (*(character + i * sizeof(int)) > 0 && isprint((char)i))
            printf("%c: %d\n", i, *(character + i * sizeof(int)));
    }
}

void create_children(int n, char* file_name, int* character, int chars_per_child)
{
    int start_idx = 0;
    int stop_idx = chars_per_child;

    while (n > 0)
    {
        if (stop_idx >= CHAR_COUNT)
            stop_idx = CHAR_COUNT - 1;

        switch (fork())
        {
            case 0:
                child_work(file_name, start_idx, stop_idx, character);
                exit(EXIT_SUCCESS);
            case -1:
                perror("Fork:");
                exit(EXIT_FAILURE);
        }
        start_idx = stop_idx + 1;
        stop_idx = start_idx + chars_per_child;
        n--;
    }
}

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s file_name N\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (argc != 3)
        usage(argv[0]);

    char* file_name = argv[1];

    int child_count = atoi(argv[2]);
    if (child_count  <= 0 || child_count > CHAR_COUNT)
        usage(argv[0]);

    int chars_per_child = CHAR_COUNT / child_count;

    int* character;
    if ((character = (int*)mmap(NULL, CHAR_COUNT * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
        ERR("mmap");
    memset(character, 0, CHAR_COUNT * sizeof(int));

    create_children(child_count, file_name, character, chars_per_child);
    parent_work(character);

    if (munmap(character, CHAR_COUNT))
        ERR("munmap");

    return EXIT_SUCCESS;
}