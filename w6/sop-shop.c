#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SHOP_FILENAME "./shop"
#define MIN_SHELVES 8
#define MAX_SHELVES 256
#define MIN_WORKERS 1
#define MAX_WORKERS 64

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

typedef struct shared_data
{
    pthread_mutex_t shelf_mutex[MAX_SHELVES];
    int shift;
    pthread_mutex_t shift_mutex;
    int alive_workers;
    int kill_count;
    pthread_mutex_t alive_workers_mutex;
}data_t;

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "\t%s n m\n", program_name);
    fprintf(stderr, "\t  n - number of items (shelves), %d <= n <= %d\n", MIN_SHELVES, MAX_SHELVES);
    fprintf(stderr, "\t  m - number of workers, %d <= m <= %d\n", MIN_WORKERS, MAX_WORKERS);
    exit(EXIT_FAILURE);
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void swap(int* x, int* y)
{
    int tmp = *y;
    *y = *x;
    *x = tmp;
}

void shuffle(int* array, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        swap(&array[i], &array[j]);
    }
}

void print_array(int* array, int n)
{
    for (int i = 0; i < n; ++i)
    {
        printf("%3d ", array[i]);
    }
    printf("\n");
}

void stock_shelves(int number_of_shelves, int* shop_data)
{
    int it = 0;
    while (it < number_of_shelves)
    {
        shop_data[it] = it + 1;
        it++;
    }
}

int check_sort(int* array, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (array[i] != i + 1)
            return -1;
    }
    return 0;
}

void manager(int shop_size, int* shop_data, data_t* shared_data)
{
    int result;
    int alive;
    int error;
    printf("[%d] Manager reports for a nightshift.\n", getpid());
    while (1)
    {
        ms_sleep(500);

        for (int i = 0; i < shop_size; i++)
        {
            if ((error = pthread_mutex_lock(&shared_data->shelf_mutex[i])) != 0)
            {
                if (error == EOWNERDEAD)
                {
                    printf("[%d] Found a dead body in aisle %d\n", getpid(), i);
                    pthread_mutex_lock(&shared_data->alive_workers_mutex);
                    shared_data->kill_count += 1;

                    if (shared_data->kill_count % 2 == 0) {
                        shared_data->alive_workers -= 1;
                    }
                    pthread_mutex_unlock(&shared_data->alive_workers_mutex);
                    pthread_mutex_consistent(&shared_data->shelf_mutex[i]);
                }
                else
                    ERR("mutex_lock");
            }
        }

        print_array(shop_data, shop_size);
        msync(shop_data, MAX_SHELVES * sizeof(int), MS_SYNC);
        result = check_sort(shop_data, shop_size);

        for (int i = 0; i < shop_size; i++)
        {
            pthread_mutex_unlock(&shared_data->shelf_mutex[i]);
        }

        pthread_mutex_lock(&shared_data->alive_workers_mutex);
        alive = shared_data->alive_workers;
        pthread_mutex_unlock(&shared_data->alive_workers_mutex);
        printf("[%d] Alive workers: %d\n", getpid(), alive);

        if (alive == 0)
            break;

        if (result == 0)
            break;
    }
    if (result == 0)
    {
        printf("[%d] The shop shelves are sorted.\n", getpid());
    }
    if (alive == 0)
    {
        printf("[%d] All workers died, I hate my job.\n", getpid());
    }
    pthread_mutex_lock(&shared_data->shift_mutex);
    shared_data->shift = 0;
    pthread_mutex_unlock(&shared_data->shift_mutex);
}

void work(int shop_size, int* shop_data, data_t* shared_data)
{
    srand(getpid());
    int error;
    printf("[%d] Worker reports for nightshift.\n", getpid());
    while (1)
    {
        pthread_mutex_lock(&shared_data->shift_mutex);
        int shift_copy = shared_data->shift;
        pthread_mutex_unlock(&shared_data->shift_mutex);

        if (shift_copy == 0)
        {
            break;
        }

        int x;
        int y;
        do
        {
            x = rand() % shop_size;
            y = rand() % shop_size;
        }while (x == y);

        if (x > y) swap(&x, &y);
        if ((error = pthread_mutex_lock(&shared_data->shelf_mutex[x])) != 0)
        {
            if (error == EOWNERDEAD)
            {
                printf("[%d] Found a dead body in aisle %d\n", getpid(), x);
                pthread_mutex_lock(&shared_data->alive_workers_mutex);
                shared_data->kill_count += 1;

                if (shared_data->kill_count % 2 == 0) {
                    shared_data->alive_workers -= 1;
                }
                pthread_mutex_unlock(&shared_data->alive_workers_mutex);
                pthread_mutex_consistent(&shared_data->shelf_mutex[x]);
            }
            else
                ERR("mutex_lock");
        }
        if ((error = pthread_mutex_lock(&shared_data->shelf_mutex[y])) != 0)
        {
            if (error == EOWNERDEAD)
            {
                printf("[%d] Found a dead body in aisle %d\n", getpid(), y);
                pthread_mutex_lock(&shared_data->alive_workers_mutex);
                shared_data->kill_count += 1;

                if (shared_data->kill_count % 2 == 0) {
                    shared_data->alive_workers -= 1;
                }
                pthread_mutex_unlock(&shared_data->alive_workers_mutex);
                pthread_mutex_consistent(&shared_data->shelf_mutex[y]);
            }
            else
                ERR("mutex_lock");
        }
        ms_sleep(100);
        if (rand() % 100 == 0)
        {
            printf("[%d] Trips over a pallet and dies.\n", getpid());
            abort();
        }
        if (shop_data[x] > shop_data[y])
            swap(&shop_data[x], &shop_data[y]);
        pthread_mutex_unlock(&shared_data->shelf_mutex[x]);
        pthread_mutex_unlock(&shared_data->shelf_mutex[y]);
    }

    munmap(shop_data, MAX_SHELVES * sizeof(int));
    munmap(shared_data, sizeof(data_t));
}

void create_workers(int worker_count, int* shop_data, data_t* shared_data, int shop_size)
{
    for (int i = 0; i < worker_count; i++)
    {
        switch (fork())
        {
            case 0:
                work(shop_size, shop_data, shared_data);
                exit(EXIT_SUCCESS);
            case -1:
                ERR("fork");
        }
    }

}

void parse_arguments(int* products, int* workers, int argc, char** argv)
{
    if (argc != 3)
        usage(argv[0]);

    *products = atoi(argv[1]);
    *workers = atoi(argv[2]);

    if (*products < MIN_SHELVES || *products > MAX_SHELVES)
        usage(argv[0]);

    if (*workers < MIN_WORKERS || *workers > MAX_WORKERS)
        usage(argv[0]);
}

int main(int argc, char** argv)
{
    int number_of_products;
    int number_of_workers;

    parse_arguments(&number_of_products, &number_of_workers, argc, argv);

    int shop_fd;
    if ((shop_fd = open(SHOP_FILENAME, O_CREAT | O_TRUNC | O_RDWR, 0666)) == -1)
        ERR("open");

    if (ftruncate(shop_fd, MAX_SHELVES * sizeof(int)) == -1)
        ERR("ftruncate");

    int* shop_data;
    if ((shop_data = mmap(NULL, MAX_SHELVES * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shop_fd, 0)) == MAP_FAILED)
        ERR("mmap");

    close(shop_fd);

    data_t* shared_data;
    if ((shared_data = (data_t*)mmap(NULL, sizeof(data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == NULL)
        ERR("mmap");

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);

    for (int i = 0; i < MAX_SHELVES; i++)
    {
        pthread_mutex_init(&shared_data->shelf_mutex[i], &mutex_attr);
    }
    pthread_mutex_init(&shared_data->shift_mutex, &mutex_attr);
    pthread_mutex_init(&shared_data->alive_workers_mutex, &mutex_attr);
    shared_data->shift = 1;
    shared_data->alive_workers = number_of_workers;
    shared_data->kill_count = 0;


    stock_shelves(number_of_products, shop_data);
    shuffle(shop_data, number_of_products);
    print_array(shop_data, number_of_products);

    create_workers(number_of_workers, shop_data, shared_data, number_of_products);
    manager(number_of_products, shop_data, shared_data);

    while (wait(NULL) > 0){}

    pthread_mutexattr_destroy(&mutex_attr);
    pthread_mutex_destroy(&shared_data->shift_mutex);
    pthread_mutex_destroy(&shared_data->alive_workers_mutex);
    for (int i = 0; i < MAX_SHELVES; i++)
    {
        pthread_mutex_destroy(&shared_data->shelf_mutex[i]);
    }
    munmap(shared_data, sizeof(data_t));
    munmap(shop_data, MAX_SHELVES * sizeof(int));
    printf("Nightshift in Bitronka is over\n");
    return EXIT_SUCCESS;
}
