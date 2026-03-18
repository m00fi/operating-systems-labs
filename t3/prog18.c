#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned int UINT;

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define RAND_DOUBLE(seedptr) ((double)rand_r(seedptr) / (double)RAND_MAX)

typedef struct argsThrow {
    pthread_t tid;
    UINT seed;
    int* pBallsThrown;
    int* pBallsWaiting;
    int* boxes;
    pthread_mutex_t *mxThrown;
    pthread_mutex_t *mxWaiting;
    pthread_mutex_t *mxBoxes;
}argsThrow_t;

void ReadArgs(int argc, char** argv, int* threadCount, int* ballCount);
int throwBall(UINT* seedptr);
void make_threads(argsThrow_t* arg_array, int threadCount);
void* thread_work(void* args);

void usage(char* argv) {
    fprintf(stderr, "Usage: %s k n\n", argv);
    fprintf(stderr, "\tk: number of threads\n");
    fprintf(stderr, "\tn: sum of balls\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    int threadCount, ballCount;
    srand(time(NULL));
    ReadArgs(argc, argv, &threadCount, &ballCount);
    pthread_mutex_t mxThrown;
    pthread_mutex_t mxWaiting;
    pthread_mutex_t mxBoxes[11];
    int boxes[11];
    int ballWaiting = ballCount;
    pthread_mutex_init(&mxThrown, NULL);
    pthread_mutex_init(&mxWaiting, NULL);
    for (int i = 0; i < 11; i++) {
        boxes[i] = 0;
        pthread_mutex_init(&mxBoxes[i], NULL);
    }
    int thrownCount = 0, bt = 0;

    argsThrow_t thread_array[threadCount];
    for (int i = 0; i < threadCount; i++) {
        thread_array[i].seed = rand();
        thread_array[i].mxWaiting = &mxWaiting;
        thread_array[i].mxBoxes = mxBoxes;
        thread_array[i].mxThrown = &mxThrown;
        thread_array[i].pBallsWaiting = &ballWaiting;
        thread_array[i].pBallsThrown = &thrownCount;
        thread_array[i].boxes = boxes;
    }

    make_threads(thread_array, threadCount);

    while (bt < ballCount) {
        sleep(1);
        pthread_mutex_lock(&mxThrown);
        bt = thrownCount;
        pthread_mutex_unlock(&mxThrown);

    }
    for (int i = 0 ; i < 11; i++) {
        printf("box[1]: ");
        for (int j = 0 ; j < boxes[i]; j++) {
            printf("*");
        }
        printf("\n");
    }

    return EXIT_SUCCESS;
}

void ReadArgs(int argc, char** argv, int* threadCount, int* ballCount) {
    if (argc != 3)
        usage(argv[0]);
    *threadCount = atoi(argv[1]);
    *ballCount = atoi(argv[2]);
    if (*threadCount < 1 || *ballCount < 1)
        usage(argv[0]);
}

int throwBall(UINT* seedptr) {
    int resultBox = 0;
    for (int i = 0; i < 11; i++) {
        if (RAND_DOUBLE(seedptr) < 0.5) {
            resultBox++;
        }
    }
    return resultBox;
}

void make_threads(argsThrow_t* arg_array, int threadCount) {
    pthread_attr_t attr;
    int err;
    if (pthread_attr_init(&attr))
        ERR("pthread_attr_init");
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
        ERR("pthread_attr_setdetachstate");

    for (int i = 0; i < threadCount; i++) {
        err = pthread_create(&arg_array[i].tid, &attr, (void*(*)(void*))thread_work, &arg_array[i]);
        if (err != 0)
            ERR("pthread_create");
    }

    pthread_attr_destroy(&attr);
}

void* thread_work(void* args) {
    argsThrow_t* arg_throw = (argsThrow_t*) args;

    while (1) {
        pthread_mutex_lock(arg_throw->mxWaiting);
        if (*arg_throw->pBallsWaiting > 0)
        {
            (*arg_throw->pBallsWaiting)--;
            pthread_mutex_unlock(arg_throw->mxWaiting);
        }
        else
        {
            pthread_mutex_unlock(arg_throw->mxWaiting);
            break;
        }

        int boxNo = throwBall(&arg_throw->seed);
        pthread_mutex_lock(&arg_throw->mxBoxes[boxNo]);
        arg_throw->boxes[boxNo]++;
        pthread_mutex_unlock(&arg_throw->mxBoxes[boxNo]);

        pthread_mutex_lock(arg_throw->mxThrown);
        (*arg_throw->pBallsThrown)++;
        pthread_mutex_unlock(arg_throw->mxThrown);
    }
    return NULL;
}