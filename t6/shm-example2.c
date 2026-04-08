#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t run = 1;

typedef struct shared_data {
    pthread_mutex_t mutex;
    int process_count;
    int total_points;
    int hit_points;
    int a;
    int b;
}shared_t;

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigint_handler(int sig){ run = 0; }

// Values of this function are in range (0,1]
double func(double x)
{
    usleep(2000);
    return exp(-x * x);
}

/**
 * It counts hit points by Monte Carlo method.
 * Use it to process one batch of computation.
 * @param N Number of points to randomize
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return Number of points which was hit.
 */
int randomize_points(int N, float a, float b)
{
    int result = 0;
    for (int i = 0; i < N; ++i)
    {
        double rand_x = ((double)rand() / RAND_MAX) * (b - a) + a;
        double rand_y = ((double)rand() / RAND_MAX);
        double real_y = func(rand_x);

        if (rand_y <= real_y)
            result++;
    }
    return result;
}

/**
 * This function calculates approximation of integral from counters of hit and total points.
 * @param total_randomized_points Number of total randomized points.
 * @param hit_points Number of hit points.
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return The approximation of integral
 */
double summarize_calculations(uint64_t total_randomized_points, uint64_t hit_points, float a, float b)
{
    return (b - a) * ((double)hit_points / (double)total_randomized_points);
}

/**
 * This function locks mutex and can sometime die (it has 2% chance to die).
 * It cannot die if lock would return an error.
 * It doesn't handle any errors. It's users responsibility.
 * Use it only in STAGE 4.
 *
 * @param mtx Mutex to lock
 * @return Value returned from pthread_mutex_lock.
 */
int random_death_lock(pthread_mutex_t* mtx)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
        return ret;

    // 2% chance to die
    if (rand() % 50 == 0)
        abort();
    return ret;
}
shared_t* open_data() {
    int data_fd;
    if ((data_fd = shm_open("shm_data", O_RDWR , 0666)) == -1) {
            ERR("shm_open");
    }

    shared_t* data;
    if ((data = (shared_t*)mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, data_fd, 0)) == MAP_FAILED)
        ERR("mmap");

    return data;
}

void work(shared_t* data, int a, int b, int N) {
    int error;
    if ((error = random_death_lock(&data->mutex)) != 0)
    {
        if (error == EOWNERDEAD)
        {
            data->process_count--;
            pthread_mutex_consistent(&data->mutex);
        }
        else
        {
            ERR("pthread_mutex_lock");
        }
    }
    if (a != data->a || b != data->b) {
        pthread_mutex_unlock(&data->mutex);
        return;
    }
    data->process_count++;
    pthread_mutex_unlock(&data->mutex);

    for (int i = 0; i < 3; i++)
    {
        int result = randomize_points(N, a, b);

        if ((error = random_death_lock(&data->mutex)) != 0)
        {
            if (error == EOWNERDEAD)
            {
                data->process_count--;
                pthread_mutex_consistent(&data->mutex);
            }
            else
            {
                ERR("pthread_mutex_lock");
            }
        }

        data->hit_points += result;
        data->total_points += N;
        printf("hit points: %d\ntotal points: %d\n", data->hit_points, data->total_points);

        pthread_mutex_unlock(&data->mutex);

        if (run == 0) break;
    }

    if ((error = random_death_lock(&data->mutex)) != 0)
    {
        if (error == EOWNERDEAD)
        {
            data->process_count--;
            pthread_mutex_consistent(&data->mutex);
        }
        else
        {
            ERR("pthread_mutex_lock");
        }
    }
    data->process_count--;
    int current_process_count = data->process_count;
    pthread_mutex_unlock(&(data->mutex));

    if (current_process_count == 0) {
        printf("Result:\n");
        if ((error = random_death_lock(&data->mutex)) != 0)
        {
            if (error == EOWNERDEAD)
            {
                data->process_count--;
                pthread_mutex_consistent(&data->mutex);
            }
            else
            {
                ERR("pthread_mutex_lock");
            }
        }
        double summary = summarize_calculations(data->total_points, data->hit_points, data->a, data->b);
        pthread_mutex_unlock(&(data->mutex));
        printf("%f\n", summary);
        pthread_mutex_destroy(&data->mutex);
        munmap(data, sizeof(shared_t));
        shm_unlink("shm_data");
        sem_unlink("shared_data");
    }
    else
        munmap(data, sizeof(shared_t));
}

void usage(char* argv[])
{
    printf("%s a b N - calculating integral with multiple processes\n", argv[0]);
    printf("a - Start of segment for integral (default: -1)\n");
    printf("b - End of segment for integral (default: 1)\n");
    printf("N - Size of batch to calculate before reporting to shared memory (default: 1000)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc != 4 && argc != 2)
        usage(argv);

    srand(getpid());
    int a, b, N;
    sethandler(sigint_handler, SIGINT);

    if (argc == 2) {
        a = -1;
        b = 1;
        N = atoi(argv[1]);
    }
    else {
        a = atoi(argv[1]);
        b = atoi(argv[2]);
        N = atoi(argv[3]);
    }

    sem_t* s = sem_open("shared_data", O_CREAT, 0666, 1);
    sem_wait(s);

    int data_fd;
    shared_t* data;

    if ((data_fd = shm_open("shm_data", O_CREAT | O_RDWR | O_EXCL, 0666)) == -1) {
        if (errno == EEXIST) {
            sem_post(s);
            data = open_data();
            work(data, a, b, N);
            exit(EXIT_SUCCESS);
        }
        else
            ERR("shm_open");
    }

    if (ftruncate(data_fd, sizeof(shared_t)) == -1)
        ERR("ftruncate");
    if ((data = (shared_t*)mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, data_fd, 0)) == MAP_FAILED)
        ERR("mmap");

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&data->mutex, &mutex_attr);

    data->process_count = 0;
    data->a = a;
    data->b = b;

    sem_post(s);

    work(data, a, b, N);

    pthread_mutexattr_destroy(&mutex_attr);

    return EXIT_SUCCESS;
}
