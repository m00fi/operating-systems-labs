#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 4096
#define DEFAULT_THREADCOUNT 10
#define DEFAULT_SAMPLESIZE 100

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;

typedef struct thread_args {
    pthread_t tid;
    UINT seed;
    int sCount;
}estimation_targs;

void* thread_work(void* args) {
    estimation_targs *t_args = (estimation_targs *) args;
    // POPRAWKA 1: Alokacja pamięci na stercie
    double *subResult = malloc(sizeof(double));
    if (subResult == NULL) {
        perror("malloc");
        pthread_exit(NULL);
    }

    int insideCount = 0;

    for (int i = 0; i < t_args->sCount; i++) {
        double x = ((double)rand_r(&t_args->seed) / (double)RAND_MAX);
        double y = ((double)rand_r(&t_args->seed) / (double)RAND_MAX);
        // POPRAWKA 4: Usunięcie sqrt (optymalizacja)
        if ((x * x + y * y) <= 1.0)
            insideCount++;
    }

    *subResult = 4.0 * (double)insideCount / (double)t_args->sCount;
    return subResult; // Zwracamy wskaźnik do pamięci na stercie
}



void usage(char* argv) {
    fprintf(stderr, "Usage: %s k n\n", argv);
    fprintf(stderr, "\tk: number of threads\n");
    fprintf(stderr, "\tn: number of coin tosses for each thread\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // POPRAWKA 3: Inicjalizacja ziarna losowości
    srand(time(NULL));

    int threadCount, samplesCount, err;
    double result = 0.0;

    // ... (parsowanie argumentów bez zmian) ...
    if (argc != 3) usage(argv[0]);
    threadCount = atoi(argv[1]);
    samplesCount = atoi(argv[2]);
    if (threadCount < 1 || samplesCount < 1) usage(argv[0]);

    estimation_targs thread_args[threadCount];

    for (int i = 0; i < threadCount; i++) {
        thread_args[i].seed = rand();
        thread_args[i].sCount = samplesCount;
    }

    for (int i = 0; i < threadCount; i++) {
        err = pthread_create(&thread_args[i].tid, NULL, thread_work, &thread_args[i]);
        if (err != 0) ERR("pthread_create");
    }

    for (int i = 0; i < threadCount; i++) {
        // POPRAWKA 2: Poprawna obsługa pthread_join
        double *threadResultPtr;

        // Przekazujemy adres wskaźnika (&threadResultPtr), aby pthread_join mogło go wypełnić
        err = pthread_join(thread_args[i].tid, (void **)&threadResultPtr);

        if (err != 0) ERR("Can't join with a thread");

        if (threadResultPtr != NULL) {
            printf("subResult = %f\n", *threadResultPtr);
            result += *threadResultPtr;
            free(threadResultPtr); // Ważne: zwalniamy pamięć zaalokowaną w wątku!
        }
    }

    result = result / threadCount;
    printf("PI ~= %f\n", result);

    return EXIT_SUCCESS;
}