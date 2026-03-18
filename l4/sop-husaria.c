#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct banner_args
{
    sem_t* sem;
    pthread_t tid;
    unsigned int seed;
    int id;
    int* enemy_hp;
    pthread_mutex_t* mx_enemy;
    pthread_cond_t* cond;

    pthread_barrier_t* b_charge;
    sem_t* lance_restock;

    int* is_rain;
    pthread_mutex_t* mx_rain;

    pthread_barrier_t* banner_run_barrier;
    pthread_mutex_t* mx_run;
    int* run;
} banner_t;

typedef struct artillery_args
{
    int* enemy_hp;
    pthread_mutex_t* mx_enemy;
    pthread_barrier_t* b_enemy;
    unsigned int seed;
    pthread_t tid;
    pthread_cond_t* cond;

    pthread_barrier_t* artillery_run_barrier;
    pthread_mutex_t* mx_run;
    int* run;
} artillery_t;

typedef struct signaller_args
{
    int* run;
    int* is_rain;
    pthread_mutex_t* mx_rain;
    pthread_mutex_t* mx_run;
    sigset_t* mask;
    pthread_barrier_t b_run;
} signaller_t;

void ms_sleep(unsigned int milli)
{
    struct timespec ts = {milli / 1000, (milli % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

void read_args(int argc, char** argv, int* banner_count, int* artillery_count);
void* banner_work(void* b_args);
void* artillery_work(void* a_args);
void* sig_work(void* sig_args);

void usage(int argc, char* argv[])
{
    printf("%s N M\n", argv[0]);
    printf("\t10 <= N <= 20 - number of banner threads\n");
    printf("\t2 <= N <= 8 - number of artillery threads\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    int err;
    int banner_count;
    int artillery_count;
    int enemy_hp = 100;
    int is_rain = 0;
    int run = 1;
    read_args(argc, argv, &banner_count, &artillery_count);

    pthread_mutex_t mx_rain;
    pthread_mutex_init(&mx_rain, NULL);
    pthread_mutex_t mx_run;
    pthread_mutex_init(&mx_run, NULL);

    pthread_barrier_t banner_run_barrier;
    pthread_barrier_init(&banner_run_barrier, NULL, banner_count);
    pthread_barrier_t artillery_run_barrier;
    pthread_barrier_init(&artillery_run_barrier, NULL, artillery_count);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    pthread_t sig_tid;
    signaller_t sig;
    sig.mx_run = &mx_run;
    sig.mx_rain = &mx_rain;
    sig.run = &run;
    sig.is_rain = &is_rain;
    sig.mask = &mask;

    err = pthread_create(&sig_tid, NULL, sig_work, &sig);
    if (err != 0)
        ERR("pthread_create");

    sem_t sem;
    sem_init(&sem, 0, 3);

    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, artillery_count);

    pthread_mutex_t mx;
    pthread_mutex_init(&mx, NULL);

    pthread_cond_t cond;
    pthread_cond_init(&cond, NULL);

    pthread_barrier_t b_charge;
    pthread_barrier_init(&b_charge, NULL, banner_count);

    sem_t lance_restock;
    sem_init(&lance_restock, 0, 4);

    banner_t banners[banner_count];
    for (int i = 0; i < banner_count; i++)
    {
        banners[i].sem = &sem;
        banners[i].seed = rand();
        banners[i].id = i + 1;
        banners[i].enemy_hp = &enemy_hp;
        banners[i].mx_enemy = &mx;
        banners[i].cond = &cond;

        banners[i].b_charge = &b_charge;
        banners[i].lance_restock = &lance_restock;

        banners[i].is_rain = &is_rain;
        banners[i].mx_rain = &mx_rain;

        banners[i].banner_run_barrier = &banner_run_barrier;
        banners[i].run = &run;
        banners[i].mx_run = &mx_run;

        err = pthread_create(&banners[i].tid, NULL, banner_work, &banners[i]);
        if (err != 0)
            ERR("pthread_create");
    }

    artillery_t artilleries[artillery_count];
    for (int i = 0; i < artillery_count; i++)
    {
        artilleries[i].b_enemy = &bar;
        artilleries[i].enemy_hp = &enemy_hp;
        artilleries[i].mx_enemy = &mx;
        artilleries[i].cond = &cond;
        artilleries[i].seed = rand();

        artilleries[i].artillery_run_barrier = &artillery_run_barrier;
        artilleries[i].run = &run;
        artilleries[i].mx_run = &mx_run;

        err = pthread_create(&artilleries[i].tid, NULL, artillery_work, &artilleries[i]);
        if (err != 0)
            ERR("pthread_create");
    }

    for (int i = 0; i < artillery_count; i++)
    {
        pthread_join(artilleries[i].tid, NULL);
    }

    for (int i = 0; i < banner_count; i++)
    {
        pthread_join(banners[i].tid, NULL);
    }

    printf("BATTLE ENDED. ENEMY HEALTH: <%3d>\n", enemy_hp);
    if (kill(0, SIGUSR2))
        ERR("kill");

    if (enemy_hp <= 0)
        printf("VENIMUS, VIDIMUS, DEUS VICIT!\n");

    pthread_join(sig_tid, NULL);

    pthread_barrier_destroy(&bar);
    pthread_mutex_destroy(&mx);
    sem_destroy(&sem);
    sem_destroy(&lance_restock);
    pthread_cond_destroy(&cond);
    pthread_barrier_destroy(&b_charge);
    return EXIT_SUCCESS;
}

void read_args(int argc, char** argv, int* banner_count, int* artillery_count)
{
    if (argc != 3)
        usage(argc, argv);

    *banner_count = atoi(argv[1]);
    *artillery_count = atoi(argv[2]);

    if (*banner_count < 10 || *banner_count > 20)
        usage(argc, argv);

    if (*artillery_count < 2 || *artillery_count > 8)
        usage(argc, argv);
}

void* banner_work(void* b_args)
{
    banner_t* b = (banner_t*)b_args;
    int s = 80 + rand_r(&b->seed) % 41;
    sem_wait(b->sem);
    ms_sleep(s);
    sem_post(b->sem);
    printf("CALAVARY <%2d>: IN POSITION\n", b->id);

    pthread_mutex_lock(b->mx_enemy);
    while ((*b->enemy_hp) >= 50)
    {
        pthread_cond_wait(b->cond, b->mx_enemy);
    }
    pthread_mutex_unlock(b->mx_enemy);

    printf("CALAVARY <%2d>: READY TO CHARGE\n", b->id);

    for (int i = 0; i < 3; i++)
    {
        pthread_barrier_wait(b->banner_run_barrier);
        pthread_mutex_lock(b->mx_run);
        int run_clone = (*b->run);
        pthread_mutex_unlock(b->mx_run);
        if (run_clone == 0)
        {
            return NULL;
        }

        pthread_mutex_lock(b->mx_rain);
        int rain_clone = (*b->is_rain);
        pthread_mutex_unlock(b->mx_rain);

        int res = pthread_barrier_wait(b->b_charge);
        if (res == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            printf("CHARGE <%d>!\n", i + 1);
        }
        ms_sleep(500);
        int miss = 1 + rand_r(&b->seed) % 10;
        if (rain_clone == 1 && miss == 1)
        {
            printf("CALAVARY <%2d>: MISSED...\n", b->id);
        }
        else
        {
            pthread_mutex_lock(b->mx_enemy);
            (*b->enemy_hp) -= 1;
            pthread_mutex_unlock(b->mx_enemy);
        }

        sem_wait(b->lance_restock);
        ms_sleep(100);
        printf("CALAVARY <%2d>: LANCE RESTOCKED\n", b->id);
        sem_post(b->lance_restock);
    }

    return NULL;
}

void* artillery_work(void* a_args)
{
    artillery_t* a = (artillery_t*)a_args;

    while (1)
    {
        pthread_mutex_lock(a->mx_enemy);
        int hp_clone = (*a->enemy_hp);
        pthread_mutex_unlock(a->mx_enemy);
        if (hp_clone < 50)
            break;

        ms_sleep(400);
        int dmg = 1 + rand_r(&a->seed) % 6;
        pthread_mutex_lock(a->mx_enemy);
        (*a->enemy_hp) -= dmg;
        pthread_mutex_unlock(a->mx_enemy);

        pthread_barrier_wait(a->artillery_run_barrier);
        pthread_mutex_lock(a->mx_run);
        int run_clone = (*a->run);
        pthread_mutex_unlock(a->mx_run);
        if (run_clone == 0)
            return NULL;

        int res = pthread_barrier_wait(a->b_enemy);
        if (res == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            pthread_mutex_lock(a->mx_enemy);
            printf("ARTILLERY: ENEMY HP <%d>\n", (*a->enemy_hp));
            pthread_mutex_unlock(a->mx_enemy);
        }

        pthread_barrier_wait(a->b_enemy);
    }

    pthread_cond_broadcast(a->cond);
    return NULL;
}

void* sig_work(void* s_args)
{
    signaller_t* s = (signaller_t*)s_args;
    int sigNo;
    sigwait(s->mask, &sigNo);
    if (sigNo == SIGUSR1)
    {
        printf("RAIN AND MUD IS SLOWING DOWN CHARGE\n");
        pthread_mutex_lock(s->mx_rain);
        (*s->is_rain) = 1;
        pthread_mutex_unlock(s->mx_rain);
    }
    else if (sigNo == SIGINT)
    {
        printf("ENDING SIMULATION...\n");
        pthread_mutex_lock(s->mx_run);
        (*s->run) = 0;
        pthread_mutex_unlock(s->mx_run);
    }
    else if (sigNo == SIGUSR2)
        return NULL;

    return NULL;
}
