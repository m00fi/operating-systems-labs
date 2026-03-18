#include <pthread.h>
#include <signal.h>

#include "risk.h"

typedef struct signal_args
{
    int* do_play;
    sigset_t* mask;
    pthread_mutex_t* mx_play;
    pthread_mutex_t* mx_regions;
    region_t* regions;
    int num_regions;
    unsigned int seed;
    int* active_players;
    pthread_mutex_t* mx_active;
} sign_t;

typedef struct player_args
{
    sign_t* signals;
    region_t* regions;
    int8_t num_regions;
    int8_t player_symbol;
    int* active_players;
    unsigned int seed;
    pthread_mutex_t* mx_region;
    pthread_mutex_t* mx_active;
    int* points;
} player_t;

void* player_thread(void* p_args);
int is_neighbour(region_t* regions, int8_t player_symbol, int idx);
int is_taken(region_t region, int8_t player_symbol);
void print_regions(region_t* regions, int num_regions, pthread_mutex_t* mx_regions);
int count_points(region_t* regions, int num_regions, int8_t player_symbol);
void* signal_thread(void* sig_args);
void signal_takeover(region_t* regions, int num_regions, unsigned int seed, pthread_mutex_t* mx_regions);

void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc != 2)
        usage(argc, argv);
    char* file_name = argv[1];
    srand(time(NULL));

    int num_regions, err;
    int active_players = 2;
    int points_a = 0;
    int points_b = 0;
    int do_play = 1;
    region_t* regions;
    regions = load_regions(file_name, &num_regions);
    pthread_mutex_t mx_regions[num_regions];
    pthread_mutex_t mx_active;
    for (int i = 0; i < num_regions; i++)
    {
        pthread_mutex_init(&mx_regions[i], NULL);
    }
    pthread_mutex_init(&mx_active, NULL);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    pthread_t sig_tid;
    sign_t sig_th;
    pthread_mutex_t mx_play;
    pthread_mutex_init(&mx_play, NULL);
    sig_th.do_play = &do_play;
    sig_th.mx_play = &mx_play;
    sig_th.mask = &mask;
    sig_th.regions = regions;
    sig_th.mx_regions = mx_regions;
    sig_th.seed = rand();
    sig_th.active_players = &active_players;
    sig_th.mx_active = &mx_active;
    err = pthread_create(&sig_tid, NULL, signal_thread, &sig_th);
    if (err != 0)
        ERR("pthread_create");

    pthread_t player_a_tid;
    player_t player_a;
    player_a.player_symbol = 'A';
    player_a.regions = regions;
    player_a.seed = rand();
    player_a.num_regions = num_regions;
    player_a.active_players = &active_players;
    player_a.mx_region = mx_regions;
    player_a.mx_active = &mx_active;
    player_a.points = &points_a;
    player_a.signals = &sig_th;

    pthread_t player_b_tid;
    player_t player_b;
    player_b.player_symbol = 'B';
    player_b.regions = regions;
    player_b.seed = rand();
    player_b.num_regions = num_regions;
    player_b.active_players = &active_players;
    player_b.mx_region = mx_regions;
    player_b.mx_active = &mx_active;
    player_b.points = &points_b;
    player_b.signals = &sig_th;

    int idx_a;
    int idx_b;
    do
    {
        idx_a = rand() % (num_regions - 1);
        idx_b = rand() % (num_regions - 1);
    } while (idx_a == idx_b);

    regions[idx_a].owner = 'A';
    regions[idx_b].owner = 'B';

    err = pthread_create(&player_a_tid, NULL, player_thread, &player_a);
    if (err != 0)
        ERR("pthread_create");
    err = pthread_create(&player_b_tid, NULL, player_thread, &player_b);
    if (err != 0)
        ERR("pthread_create");

    int active_copy;
    while (1)
    {
        pthread_mutex_lock(&mx_active);
        active_copy = active_players;
        pthread_mutex_unlock(&mx_active);

        if (active_copy == 0)
            break;

        ms_sleep(SHOW_MS);
        print_regions(regions, num_regions, mx_regions);
    }

    pthread_join(player_a_tid, NULL);
    pthread_join(player_b_tid, NULL);
    if (kill(0, SIGUSR1))
        ERR("kill");
    pthread_join(sig_tid, NULL);

    if (do_play == 0)
    {
        printf("Robin Hood won!\n");
    }
    else
    {
        print_regions(regions, num_regions, mx_regions);
        int a_p = count_points(regions, num_regions, 'A');
        int b_p = count_points(regions, num_regions, 'B');
        printf("A: %d points\nB: %d points\n", a_p, b_p);
    }

    free(regions);
    return EXIT_SUCCESS;
}

void* player_thread(void* p_args)
{
    player_t* p = (player_t*)p_args;
    int frustration = 0;

    while (1)
    {
        pthread_mutex_lock(p->signals->mx_play);
        int play_copy = (*p->signals->do_play);
        pthread_mutex_unlock(p->signals->mx_play);
        if (play_copy == 0)
        {
            pthread_mutex_lock(p->mx_active);
            (*p->active_players)--;
            pthread_mutex_unlock(p->mx_active);
            break;
        }

        int takeover_idx = rand_r(&p->seed) & (p->num_regions - 1);
        ms_sleep(MOVE_MS);
        pthread_mutex_lock(&p->mx_region[takeover_idx]);
        if (is_taken(p->regions[takeover_idx], p->player_symbol))
        {
            printf("[%c] region %d is taken!\n", p->player_symbol, takeover_idx);
            frustration++;
        }
        else if (!is_neighbour(p->regions, p->player_symbol, takeover_idx))
        {
            printf("[%c] region %d is not neighbouring with any of players regions!\n", p->player_symbol, takeover_idx);
            frustration++;
        }
        else
        {
            p->regions[takeover_idx].owner = p->player_symbol;
            (*p->points)++;
            frustration = 0;
        }
        pthread_mutex_unlock(&p->mx_region[takeover_idx]);
        if (frustration >= FRUSTRATION_LIMIT)
            break;
    }
    pthread_mutex_lock(p->mx_active);
    (*p->active_players)--;
    pthread_mutex_unlock(p->mx_active);

    return NULL;
}

int is_neighbour(region_t* regions, int8_t player_symbol, int idx)
{
    region_t r = regions[idx];
    for (int i = 0; i < r.num_neighbors; i++)
    {
        int n = r.neighbors[i];
        if (regions[n].owner == player_symbol)
            return 1;
    }
    return 0;
}

int is_taken(region_t region, int8_t player_symbol)
{
    if (region.owner != player_symbol)
        return 0;
    return 1;
}

void print_regions(region_t* regions, int num_regions, pthread_mutex_t* mx_regions)
{
    for (int i = 0; i < num_regions; i++)
    {
        pthread_mutex_lock(&mx_regions[i]);
    }
    for (int i = 0; i < num_regions; i++)
    {
        printf("%d [%c] : ", i, regions[i].owner);
        for (int j = 0; j < regions[i].num_neighbors; j++)
        {
            printf("%d;", regions[i].neighbors[j]);
        }
        printf("\n");
    }
    for (int i = 0; i < num_regions; i++)
    {
        pthread_mutex_unlock(&mx_regions[i]);
    }
}

int count_points(region_t* regions, int num_regions, int8_t player_symbol)
{
    int points = 0;
    for (int i = 0; i < num_regions; i++)
    {
        if (regions[i].owner == player_symbol)
            points++;
    }
    return points;
}

void* signal_thread(void* sig_args)
{
    sign_t* s = (sign_t*)sig_args;
    int sigNo;
    while (1)
    {
        pthread_mutex_lock(s->mx_active);
        int active_copy = (*s->active_players);
        pthread_mutex_unlock(s->mx_active);

        if (active_copy == 0)
            break;
        sigwait(s->mask, &sigNo);
        if (sigNo == SIGINT)
        {
            signal_takeover(s->regions, s->num_regions, s->seed, s->mx_regions);
        }
        else if (sigNo == SIGTERM)
        {
            pthread_mutex_lock(s->mx_play);
            s->do_play = 0;
            pthread_mutex_unlock(s->mx_play);
            break;
        }
        else if (sigNo == SIGUSR1)
        {
            break;
        }
    }
    return NULL;
}

void signal_takeover(region_t* regions, int num_regions, unsigned int seed, pthread_mutex_t* mx_regions)
{
    int idx = 0;
    do
    {
        idx = rand_r(&seed) % (num_regions - 1);
    } while (regions[idx].owner == '-');

    pthread_mutex_lock(&mx_regions[idx]);
    regions[idx].owner = '-';
    pthread_mutex_unlock(&mx_regions[idx]);
}
