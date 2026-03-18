#include "common.h"

typedef struct sigh
{
    int* do_work;
    pthread_mutex_t* mx_do_work;
    sigset_t mask;
}sigh_t;

typedef struct plot
{
    int idx;
    pthread_mutex_t mx_plot;
    pthread_cond_t delivery;
    int delivered_salt;
    int salted_iugerum;
    sigh_t* signal;
}plot_t;

typedef struct carrier_args
{
    plot_t* plot;
    unsigned int seed;
    sigh_t* signal;
    int plot_count;
}carrier_t;

void* sighandler_thread(void* arg);
void read_args(int argc, char** argv, int* N, int* Q);
void carrier_init(carrier_t* c_args, int carrier_count, pthread_t* carrier_tid);
void worker_init(plot_t* plot, int worker_count, pthread_t* worker_tid);
void* carrier_thread(void* arg);
void* worker_thread(void* arg);

int main(int argc, char* argv[])
{
    srand(time(NULL));
    int err;
    int do_work = 1;
    pthread_mutex_t do_work_mutex = PTHREAD_MUTEX_INITIALIZER;

    int plot_count, carrier_count;
    read_args(argc, argv, &plot_count, &carrier_count);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) ERR("pthread_sigmask");

    sigh_t handler_args;
    handler_args.do_work = &do_work;
    handler_args.mx_do_work = &do_work_mutex;
    handler_args.mask = mask;

    pthread_t sig_tid;
    err = pthread_create(&sig_tid, NULL, sighandler_thread, &handler_args);
    if (err != 0) ERR("pthread_create");

    plot_t plot[plot_count];
    for (int i = 0; i < plot_count; i++)
    {
        plot[i].signal = &handler_args;
        plot[i].delivered_salt = 0;
        plot[i].salted_iugerum = 0;
        plot[i].idx = i+1;
        pthread_mutex_init(&plot[i].mx_plot, NULL);
        if (pthread_cond_init(&plot[i].delivery, NULL) != 0) ERR("cond init");
    }
    pthread_t carrier_tid[carrier_count];
    carrier_t carrier[carrier_count];
    for (int i = 0; i < carrier_count; i++)
    {
        carrier[i].signal = &handler_args;
        carrier[i].plot = plot;
        carrier[i].seed = rand();
        carrier[i].plot_count = plot_count;
    }
    carrier_init(carrier, carrier_count, carrier_tid);

    pthread_t worker_tid[plot_count];
    worker_init(plot, plot_count, worker_tid);

    for (int i = 0; i < plot_count; i++)
        pthread_join(worker_tid[i], NULL);

    printf("SCIPIO: VENI, VIDI, VICI, CONSPERSI.\n");
    if (kill(0, SIGINT)) ERR("kill");

    for (int i = 0; i < carrier_count; i++)
        pthread_join(carrier_tid[i], NULL);

    pthread_join(sig_tid, NULL);




    for (int i = 0; i < plot_count; i++)
    {
        pthread_mutex_destroy(&plot[i].mx_plot);
    }
    pthread_mutex_destroy(&do_work_mutex);

    return EXIT_SUCCESS;
}

void* sighandler_thread(void* args)
{
    sigh_t* sh = (sigh_t*)args;
    int sigNo;

    sigwait(&sh->mask, &sigNo);
    if (sigNo == SIGINT)
    {
        pthread_mutex_lock(sh->mx_do_work);
        (*sh->do_work) = 0;
        pthread_mutex_unlock(sh->mx_do_work);
    }
    return NULL;
}

void read_args(int argc, char** argv, int* N, int* Q)
{
    if (argc != 3) usage(argc, argv);
    *N = atoi(argv[1]);
    *Q = atoi(argv[2]);
    if (*N < 1 || *N > 20) usage(argc, argv);
    if (*Q < 1 || *Q > 10) usage(argc, argv);
}

void carrier_init(carrier_t* c_args, int carrier_count, pthread_t* carrier_tid)
{
    int err;
    for (int i = 0; i < carrier_count; i++)
    {
        err = pthread_create(&carrier_tid[i], NULL, carrier_thread, &c_args[i]);
        if (err != 0) ERR("pthread_create");
    }
}

void* carrier_thread(void* arg)
{
    carrier_t* c = (carrier_t*)arg;
    int do_work_copy = 1;
    int idx = -1;
    while (1)
    {
        pthread_mutex_lock(c->signal->mx_do_work);
        do_work_copy = (*c->signal->do_work);
        pthread_mutex_unlock(c->signal->mx_do_work);
        if (!do_work_copy) break;

        idx = rand_r(&c->seed) % c->plot_count;
        msleep(5 + idx);
        pthread_mutex_lock(&c->plot[idx].mx_plot);
        c->plot[idx].delivered_salt += 5;
        pthread_cond_signal(&c->plot[idx].delivery);
        pthread_mutex_unlock(&c->plot[idx].mx_plot);
    }

    return NULL;
}

void worker_init(plot_t* plot, int worker_count, pthread_t* worker_tid)
{
    int err;
    for (int i = 0; i < worker_count; i++)
    {
        err = pthread_create(&worker_tid[i], NULL, worker_thread, &plot[i]);
        if (err != 0) ERR("pthread_create");
    }
}

void* worker_thread(void* arg)
{
    plot_t* p = (plot_t*)arg;
    int do_work_copy = 1;
    int err;

    while (1)
    {
        pthread_mutex_lock(p->signal->mx_do_work);
        do_work_copy = (*p->signal->do_work);
        pthread_mutex_unlock(p->signal->mx_do_work);
        if (!do_work_copy) break;

        pthread_mutex_lock(&p->mx_plot);
        if (p->delivered_salt < 1)
        {
            while (1)
            {
                pthread_mutex_lock(p->signal->mx_do_work);
                do_work_copy = (*p->signal->do_work);
                pthread_mutex_unlock(p->signal->mx_do_work);

                if (!do_work_copy) {
                    pthread_mutex_unlock(&p->mx_plot);
                    return NULL;
                }
                struct timespec sleep = get_cond_wait_time(90);
                err = pthread_cond_timedwait(&p->delivery, &p->mx_plot, &sleep);
                if (err == ETIMEDOUT)
                    printf("SERVVS %d: EXSPECTO SALEM.\n", p->idx);
                else
                    break;
            }
        }
        msleep(15);
        p->delivered_salt -= 1;
        p->salted_iugerum += 1;
        printf("SERVVS %d: ADIPISCOR SALEM %d ADHVC IVGERVM.\n",p->idx, p->salted_iugerum);
        if (p->salted_iugerum == 50)
        {
            printf("SERVVS %d: LABOR MEVS CONFICIO.\n", p->idx);
            pthread_mutex_unlock(&p->mx_plot);
            break;
        }
        pthread_mutex_unlock(&p->mx_plot);
    }

    return NULL;
}
