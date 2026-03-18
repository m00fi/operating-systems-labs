#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BSIZE 500

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

void usage(char* name) {
    fprintf(stderr, "Usage: %s n\n", name);
    fprintf(stderr, "n - number of students\n");
    exit(EXIT_FAILURE);
}

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milisec * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}


void teacher_work(int n, int* student_fds, int read_fd, pid_t* student_pids) {
    srand(getpid());

    ssize_t status = 0;
    char buf[BSIZE];

    for (int i = 0; i < n; i++) {
        memset(buf, 0, BSIZE);
        sprintf(buf, "Teacher: Is %d here?\n", student_pids[i]);
        printf("%s", buf);

        if ((status = write(student_fds[i], buf, BSIZE)) != BSIZE)
            ERR("write");
        if ((status = read(read_fd, buf, BSIZE)) != BSIZE)
            ERR("write");
    }

    int stage[4];
    stage[0] = 3;
    stage[1] = 6;
    stage[2] = 7;
    stage[3] = 5;

    while (1) {
        if ((status = read(read_fd, buf, BSIZE)) != BSIZE) {
            if (status == 0)
                break;
            ERR("read");
        }

        char* _pid = strtok(buf, "_");
        char* _try = strtok(NULL, "_");
        char* _curr_stage = strtok(NULL, "_");

        int pid = atoi(_pid);
        int try = atoi(_try);
        int curr_stage = atoi(_curr_stage);

        memset(buf, 0, BSIZE);
        int idx = -1;

        for (int i = 0; i < n; i++) {
            if (pid == student_pids[i]) {
                idx = i;
                break;
            }
        }
        //printf("DEBUG\n");

        if (try >= stage[curr_stage - 1] + 1 + rand() % 20) {
            printf("Teacher: Student %d finished stage %d\n", pid, curr_stage);

            sprintf(buf, "%d", 1);
            if ((status = write(student_fds[idx], buf, BSIZE)) != BSIZE)
                ERR("write");
        }
        else {
            printf("Teacher: Student %d needs to fix stage %d\n", pid, curr_stage);

            sprintf(buf, "%d", 0);
            if ((status = write(student_fds[idx], buf, BSIZE)) != BSIZE)
                ERR("write");
        }
    }
}

void student_work(int read_fd, int write_fd) {
    char attendance[BSIZE];
    memset(attendance, 0, BSIZE);
    sprintf(attendance, "Teacher: Is %d here?\n", getpid());

    char buf[BSIZE];
    ssize_t status = 0;
    if ((status = read(read_fd, buf, BSIZE)) != BSIZE)
        ERR("read");

    if (strcmp(attendance, buf) == 0) {
        memset(buf, 0, BSIZE);
        sprintf(buf, "Student %d: HERE\n", getpid());
        printf("%s", buf);
        if ((status = write(write_fd, buf, BSIZE)) != BSIZE)
            ERR("write");
    }

    srand(getpid());
    int k = 3 + rand() % 7;
    int curr_stage = 1;

    while (curr_stage <= 4) {
        int t = 100 + rand() % 401;
        msleep(t);
        int q = 1 + rand() % 20;
        int try = k + q;
        memset(buf, 0, BSIZE);
        sprintf(buf, "%d_%d_%d", getpid(), try, curr_stage);

        if ((status = write(write_fd, buf, BSIZE)) != BSIZE)
            ERR("write");

        if ((status = read(read_fd, buf, BSIZE)) != BSIZE)
            ERR("read");

        int pass = atoi(buf);
        curr_stage += pass;
    }

    printf("Student %d: I NAILED IT!\n", getpid());
}

void make_students(int n, int* student_fds, int* teacher_fd, pid_t* student_pids) {
    int temp_fd[2];


    for (int i = 0; i < n; i++) {
        if (pipe(temp_fd) == -1)
            ERR("pipe");

        student_fds[i] = temp_fd[1];
        switch (student_pids[i] = fork()) {
            case -1:
                ERR("fork()");
            case 0:
                for (int j = 0; j < i; j++) {
                    close(student_fds[j]);
                }

                close(temp_fd[1]);
                close(teacher_fd[0]);
                free(student_fds);
                free(student_pids);

                student_work(temp_fd[0], teacher_fd[1]);

                close(temp_fd[0]);
                close(teacher_fd[1]);
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
    close(teacher_fd[1]);
}

int main(int argc, char** argv) {
    if (argc != 2)
        usage(argv[0]);

    int n = atoi(argv[1]);
    if (n < 3 || n > 20)
        usage(argv[0]);

    if (sethandler(SIG_IGN, SIGPIPE) == -1)
        ERR("sethandler");

    int teacher_fd[2];
    if (pipe(teacher_fd) == -1)
        ERR("pipe");

    int* student_fds = malloc(n * sizeof(int));
    if (!student_fds)
        ERR("malloc");
    pid_t* student_pids = malloc(n * sizeof(pid_t));
    if (!student_pids)
        ERR("malloc");

    int teacher_read_fd = teacher_fd[0];

    make_students(n, student_fds, teacher_fd, student_pids);
    teacher_work(n, student_fds, teacher_read_fd, student_pids);

    while (wait(NULL) > 0){}

    printf("Teacher: IT'S FINALLY OVER!\n");

    free(student_pids);
    free(student_fds);
    return EXIT_SUCCESS;
}
