#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef unsigned int UINT;

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct argSignal {
    pthread_t tid;
    int* arrayCount;
    int* stopFlag;
    int* itemArray;
    pthread_mutex_t* mxStopFlag;
    pthread_mutex_t* mxItemArray;
    sigset_t* mask;
}argSignal_t;

void ReadArguments(int argc, char** argv, int* k);
void removeItem(int* itemArray, int* arrayCount, UINT* seed, int k);
void printArray(int* itemArray, int k);
void* thread_work(void* args);

void usage(char* argv) {
    fprintf(stderr, "Usage: %s k\n", argv);
    fprintf(stderr, "\tk: list of numbers from 1 to k\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int k;
    int stopFlag = 0;
    ReadArguments(argc, argv, &k);
    int arrayCount = k;
    int itemArray[k];
    for (int i = 0; i < k; i++) {
        itemArray[i] = i + 1;
    }
    pthread_mutex_t mxStopFlag, mxItemArray;
    pthread_mutex_init(&mxStopFlag, NULL);
    pthread_mutex_init(&mxItemArray, NULL);
    argSignal_t args;
    args.arrayCount = &arrayCount;
    args.stopFlag = &stopFlag;
    args.itemArray = itemArray;
    args.mxItemArray = &mxItemArray;
    args.mxStopFlag = &mxStopFlag;

    sigset_t newMask, oldMask;
    sigfillset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGQUIT);
    args.mask = &newMask;
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask)) {
        ERR("pthread_sigmask");
    }

    if (pthread_create(&args.tid, NULL, thread_work, (void*)&args))
        ERR("pthread_create");

    while (1) {
        sleep(1);
        pthread_mutex_lock(&mxStopFlag);
        if (stopFlag) {
            pthread_mutex_unlock(&mxStopFlag);
            break;
        }
        pthread_mutex_unlock(&mxStopFlag);

        pthread_mutex_lock(&mxItemArray);
        printArray(itemArray, k);
        pthread_mutex_unlock(&mxItemArray);
    }

    if (pthread_join(args.tid, NULL))
        ERR("Can't join with 'signal handling' thread");
    if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char **argv, int *k)
{
    if (argc != 2) usage(argv[0]);
    *k = atoi(argv[1]);
    if (*k < 1) usage(argv[0]);
}

void removeItem(int* itemArray, int* arrayCount, UINT* seed, int k) {
    int idx = rand_r(seed) % k;
    while (itemArray[idx] == -1) {
        idx = rand_r(seed) % k;
    }
    itemArray[idx] = -1;
    (*arrayCount)--;
}

void printArray(int* itemArray, int k) {
    for (int i = 0; i < k; i++) {
        if (itemArray[i] != -1) {
            printf("%d ", itemArray[i]);
        }
        else {
            printf("* ");
        }
    }
    printf("\n");
}

void* thread_work(void* args) {
    argSignal_t* arg = (argSignal_t*)args;
    int signum;
    int k = *arg->arrayCount;
    UINT seed = rand();
    for (;;) {
        if (sigwait(arg->mask, &signum))
            ERR("sigwait");
        switch (signum) {
            case SIGINT:
                pthread_mutex_lock(arg->mxItemArray);
                if (*arg->arrayCount > 0)
                    removeItem(arg->itemArray, arg->arrayCount, &seed, k);
                pthread_mutex_unlock(arg->mxItemArray);
                break;
            case SIGQUIT:
                pthread_mutex_lock(arg->mxStopFlag);
                *arg->stopFlag = 1;
                pthread_mutex_unlock(arg->mxStopFlag);
                return NULL;
            default:
                break;
        }
    }
}