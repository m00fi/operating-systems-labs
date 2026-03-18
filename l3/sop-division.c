#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct thread_data {
    int* L;                 // Wskaźnik na wspólny licznik
    int* readCount;         // Ile wątków już obsłużyło aktualne L
    int totalThreads;       // Ilu jest wszystkich studentów/wątków
    pthread_mutex_t* mxL;   // Mutex chroniący L i readCount
    unsigned int seed;
    pthread_t tid;
} data_t;

// Flaga sterująca działaniem pętli
volatile sig_atomic_t run = 1;

void sigint_handler(int sig) {
    run = 0;
}

void read_arg(int argc, char** argv, int* number_of_threads);
void create_threads(int number_of_threads, data_t* data);
void* thread_work(void* data);
void msleep(unsigned int milisec);

void usage(char* argv) {
    fprintf(stderr, "Usage: %s n\n", argv);
    fprintf(stderr, "\tn: no. of threads\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    int L = 1;
    int readCount = 0;
    int number_of_threads, err;

    srand(time(NULL));
    read_arg(argc, argv, &number_of_threads);

    // Alokujemy dynamicznie, żeby nie zapchać stosu przy dużej liczbie wątków
    data_t* data = malloc(sizeof(data_t) * number_of_threads);
    if (!data) ERR("malloc");

    pthread_mutex_t mxL;
    if (pthread_mutex_init(&mxL, NULL) != 0) ERR("mutex init");

    // Obsługa sygnałów
    sigset_t thMask, oMask;
    sigemptyset(&thMask);
    sigaddset(&thMask, SIGINT);
    // Blokujemy SIGINT przed utworzeniem wątków (dziedziczenie maski)
    pthread_sigmask(SIG_BLOCK, &thMask, &oMask);

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Inicjalizacja struktur danych
    for (int i = 0; i < number_of_threads; i++) {
        data[i].L = &L;
        data[i].mxL = &mxL;
        data[i].readCount = &readCount;
        data[i].totalThreads = number_of_threads;
    }

    create_threads(number_of_threads, data);

    // Odblokowanie SIGINT w wątku głównym
    pthread_sigmask(SIG_UNBLOCK, &thMask, NULL);

    // --- PĘTLA GŁÓWNA ---
    while (run) {
        // 1. Czekaj 0.1 sekundy (zgodnie z zadaniem)
        msleep(100);

        // 2. Czekaj, aż WSZYSTKIE wątki przetworzą poprzednie L
        while (run) {
            pthread_mutex_lock(&mxL);
            if (readCount == number_of_threads) {
                // Wszyscy gotowi, można zmieniać L
                L++;
                readCount = 0; // Resetujemy licznik przeczytań dla nowego L
                pthread_mutex_unlock(&mxL);
                break; // Wychodzimy z pętli czekania, wracamy do msleep(100)
            }
            pthread_mutex_unlock(&mxL);

            // WAŻNE: Krótkie spanie, żeby nie zająć 100% CPU
            // Dajemy szansę wątkom roboczym na zdobycie mutexa
            usleep(1000);
        }
    }

    printf("\nMain: Otrzymano sygnał, kończenie pracy...\n");

    // --- Czekanie na wątki ---
    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(data[i].tid, NULL);
    }

    pthread_mutex_destroy(&mxL);
    free(data);
    return EXIT_SUCCESS;
}

void read_arg(int argc, char** argv, int* number_of_threads) {
    if (argc != 2) usage(argv[0]);
    *number_of_threads = atoi(argv[1]);
    if (*number_of_threads <= 0) usage(argv[0]);
}

void create_threads(int number_of_threads, data_t* data) {
    for (int i = 0; i < number_of_threads; i++) {
        data[i].seed = rand();
        if (pthread_create(&data[i].tid, NULL, thread_work, &data[i]) != 0)
            ERR("pthread_create");
    }
}

void* thread_work(void* arg) {
    data_t* th_data = (data_t*)arg;
    unsigned int seed = th_data->seed;
    int M = (rand_r(&seed) % 99) + 2; // Mnożnik [2-100]

    // Lokalna zmienna, żeby pamiętać, które L już sprawdziliśmy
    // Zaczynamy od 1, bo L startuje od 1, a my chcemy je sprawdzić
    // Ale w naszej logice czekamy na zmianę, więc ustawiamy 0,
    // żeby w pierwszym przebiegu L (1) != last_L (0)
    int last_L = 0;

    while (run) {
        pthread_mutex_lock(th_data->mxL);

        int current_L = *th_data->L;

        // Sprawdzamy, czy L się zmieniło w porównaniu do tego, co widzieliśmy ostatnio
        if (current_L != last_L) {

            // --- LOGIKA SPRAWDZANIA ---
            if (current_L % M == 0) {
                printf("\nWątek %lu (M=%d): %d jest podzielne przez %d",
                       pthread_self(), M, current_L, M);
            }

            // Oznaczamy, że ten wątek obsłużył to L
            (*th_data->readCount)++;
            last_L = current_L; // Zapamiętujemy, żeby nie sprawdzać tego samego L dwa razy

            pthread_mutex_unlock(th_data->mxL);
        }
        else {
            // Jeśli L jest stare (main jeszcze nie zwiększył), to czekamy
            pthread_mutex_unlock(th_data->mxL);

            // WAŻNE: Oddajemy procesor, bo inaczej zagłodzimy Maina
            usleep(1000); // 1ms
        }
    }

    return NULL;
}

void msleep(unsigned int milisec) {
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    nanosleep(&req, NULL);
}