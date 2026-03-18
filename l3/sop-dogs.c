#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h> // Dodane dla time()

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

volatile sig_atomic_t run = 1;

void sigint_handler(int signum) {
    run = 0;
}

typedef struct common {
    int* track;
    int* track_length;
    pthread_mutex_t* mxtrack;
    int* three_fastest_idx;
    int* finished_count;
    pthread_mutex_t* mxFinish; // NOWY: Mutex do ochrony licznika
} common_t;

typedef struct dog_args {
    common_t* common;
    pthread_t tid;
    int dog_number;
    unsigned int seed;
} dog_t;

void sethandler(void (*f)(int), int sigNo);
void read_args(int argc, char** argv, int* track_length, int* dog_count);
void start_race(int dog_count, dog_t* dog_args);
void* dog_work(void* args);
void print_track(int* track, int* track_length);
void msleep(unsigned int milisec);

void usage(char* argv) {
    fprintf(stderr, "Usage: %s n m\n", argv);
    fprintf(stderr, "\tn>20: track length\n");
    fprintf(stderr, "\tm>2: number of dogs\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    int track_length, dog_count, err;
    int finished_count = 0;
    int interrupted = 0;
    read_args(argc, argv, &track_length, &dog_count);
    srand(time(NULL));

    int* track = (int*) malloc(sizeof(int) * track_length);
    if (!track) ERR("malloc track");

    pthread_mutex_t* mxtrack = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t) * track_length);
    if (!mxtrack) ERR("malloc mutexes");

    int three_fastest[3] = {0}; // Warto wyzerować

    // Inicjalizacja mutexów toru
    for (int i = 0; i < track_length; i++) {
        track[i] = 0;
        if (pthread_mutex_init(&mxtrack[i], NULL) != 0) ERR("mutex init");
    }

    // NOWY: Mutex dla licznika mety
    pthread_mutex_t mxFinish = PTHREAD_MUTEX_INITIALIZER;

    common_t common;
    common.track = track;
    common.track_length = &track_length;
    common.mxtrack = mxtrack;
    common.three_fastest_idx = three_fastest;
    common.finished_count = &finished_count;
    common.mxFinish = &mxFinish; // Przekazanie mutexa

    // Użycie VLA (dog[dog_count]) jest ryzykowne dla dużych m,
    // ale na potrzeby zadania zazwyczaj akceptowalne.
    // Bezpieczniej byłoby: dog_t* dog = malloc(...)
    dog_t dog[dog_count];
    for (int i = 0; i < dog_count; i++) {
        dog[i].common = &common;
        dog[i].seed = rand();
        dog[i].dog_number = i + 1;
    }

    sigset_t mask, omask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &mask, &omask) != 0) ERR("pthread_sigmask");

    start_race(dog_count, dog);

    sethandler(sigint_handler, SIGINT);
    if (pthread_sigmask(SIG_UNBLOCK, &mask, &omask) != 0) ERR("pthread_sigmask");

    while (1) {
        msleep(1000);
        print_track(track, &track_length);

        // POPRAWKA: Bezpieczny odczyt licznika
        pthread_mutex_lock(&mxFinish);
        int current_finished = finished_count;
        pthread_mutex_unlock(&mxFinish);

        if (current_finished >= dog_count) break;

        if (!run) {
            interrupted = 1;
            break;
        }
    }

    if (interrupted) {
        printf("Race has been interrupted!\n");
        for (int i = 0; i < dog_count; i++) {
            err = pthread_cancel(dog[i].tid);
            if (err != 0) ERR("pthread_cancel");
        }
    }

    for (int i = 0; i < dog_count; i++) {
        err = pthread_join(dog[i].tid, NULL);
        if (err != 0) ERR("pthread_join");
    }
    if (!interrupted) {
        printf("Three fastest dogs were:\n");
        printf("\t1st place: %d\n", three_fastest[0]);
        printf("\t2nd place: %d\n", three_fastest[1]);
        printf("\t3rd place: %d\n", three_fastest[2]);
    }


    // POPRAWKA: Sprzątanie mutexów
    for (int i = 0; i < track_length; i++) {
        pthread_mutex_destroy(&mxtrack[i]);
    }
    pthread_mutex_destroy(&mxFinish);

    free(track);
    free(mxtrack);
    return EXIT_SUCCESS;
}

// ... read_args bez zmian ...
void read_args(int argc, char** argv, int* track_length, int* dog_count) {
    if (argc != 3) usage(argv[0]);
    *track_length = atoi(argv[1]);
    *dog_count = atoi(argv[2]);
    if (*track_length <= 20 || *dog_count <= 2) usage(argv[0]);
}

// ... start_race bez zmian ...
void start_race(int dog_count, dog_t* dog_args) {
    int err;
    for (int i = 0; i < dog_count; i++) {
        err = pthread_create(&dog_args[i].tid, NULL, dog_work, &dog_args[i]);
        if (err != 0) ERR("pthread_create");
    }
}

void* dog_work(void* args) {
    dog_t* arg = (dog_t*) args;

    // Inicjalizacja pozycji startowej
    int current_idx = 0;
    int state;
    pthread_mutex_lock(&arg->common->mxtrack[current_idx]);
    arg->common->track[current_idx]++;
    pthread_mutex_unlock(&arg->common->mxtrack[current_idx]);

    unsigned int seed = arg->seed; // Kopia lokalna dla wygody
    int jump_length = rand_r(&seed) % 5 + 1;
    int sleep_time = rand_r(&seed) % 1321 + 200;

    for (;;) {
        msleep(sleep_time);
        sleep_time = rand_r(&seed) % 1321 + 200;

        int next_idx = current_idx + jump_length;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
        // --- WARUNEK METY ---
        if (next_idx >= (*arg->common->track_length) - 1) {
            next_idx = (*arg->common->track_length) - 1;

            // Blokujemy mutexy: Mniejszy indeks, potem większy (tu zawsze current < next)
            pthread_mutex_lock(&arg->common->mxtrack[current_idx]);
            pthread_mutex_lock(&arg->common->mxtrack[next_idx]);
            // Uwaga: kolejność current -> next jest OK, bo idziemy tylko w prawo

            arg->common->track[current_idx]--;
            arg->common->track[next_idx]++;

            // Sprawdzenie podium (chronione mutexem pola mety - to jest OK)
            // track[next_idx] przechowuje liczbę psów na mecie
            int position = arg->common->track[next_idx];
            if (position <= 3) {
                arg->common->three_fastest_idx[position - 1] = arg->dog_number;
            }

            pthread_mutex_unlock(&arg->common->mxtrack[next_idx]);
            pthread_mutex_unlock(&arg->common->mxtrack[current_idx]);

            // POPRAWKA: Inkrementacja licznika pod osobnym mutexem
            pthread_mutex_lock(arg->common->mxFinish);
            (*arg->common->finished_count)++;
            pthread_mutex_unlock(arg->common->mxFinish);

            break; // Koniec pracy wątku
        }

        // --- NORMALNY RUCH ---
        // Kolejność blokowania: current < next, więc najpierw current
        pthread_mutex_lock(&arg->common->mxtrack[current_idx]);
        pthread_mutex_lock(&arg->common->mxtrack[next_idx]);

        if (arg->common->track[next_idx] == 0) {
            arg->common->track[current_idx]--;
            arg->common->track[next_idx]++;

            printf("[%d] new position: %d\n", arg->dog_number, next_idx);

            pthread_mutex_unlock(&arg->common->mxtrack[next_idx]);
            pthread_mutex_unlock(&arg->common->mxtrack[current_idx]);

            current_idx = next_idx; // Aktualizacja pozycji PO odblokowaniu
        } else {
            printf("[%d] woof\n", arg->dog_number);

            pthread_mutex_unlock(&arg->common->mxtrack[next_idx]);
            pthread_mutex_unlock(&arg->common->mxtrack[current_idx]);
            // Pozycja się nie zmienia
        }
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);

        jump_length = rand_r(&seed) % 5 + 1;
    }
    return NULL;
}

// ... print_track i msleep bez zmian ...
void print_track(int* track, int* track_length) {

    printf("[ ");
    for (int i = 0; i < *track_length; i++) {
        printf("%d ", track[i]);
    }
    printf("]\n");
}

void msleep(unsigned int milisec) {
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    nanosleep(&req, NULL);
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}