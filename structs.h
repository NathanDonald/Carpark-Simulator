#pragma once
#include <pthread.h>

int shm_fd;

#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5
#define CARPARKS_PER_LEVEL 20

#define EMPTY_QUEUE "EMPTY-"
#define EMPTY_LPR "EMPTY-"

#define SHARE_NAME "PARKING"
#define SHMSZ 2920

typedef struct lpr
{
    pthread_mutex_t m;
    pthread_cond_t c;
    char rego[6];
} lpr_t;

typedef struct boomgate
{
    pthread_mutex_t m;
    pthread_cond_t c;
    char s;
} boomgate_t;

typedef struct tempnode
{
    short temperature;
} tempnode_t;

typedef struct parking_sign
{
    pthread_mutex_t m;
    pthread_cond_t c;
    char display;
} parking_sign_t;

typedef struct entry
{
    lpr_t lpr;
    boomgate_t boomgate;
    parking_sign_t parkingsign;
} entry_t;

typedef struct exit
{
    lpr_t lpr;
    boomgate_t boomgate;
} exit_t;

typedef struct level
{
    lpr_t lpr;
    tempnode_t tempnode;
    // unsure if alarm is char??
    char alarm;
} level_t;

typedef struct shared_data
{
    // char check;
    // Memory for each of the 5 enterances
    entry_t entries[ENTRANCES];

    // Memory for each of the 5 exits
    exit_t exits[EXITS];

    // Memory for each of the 5 levels
    level_t levels[LEVELS];

} shared_data_t;

typedef struct shared_memory
{
    const char *name;
    int fd;
    shared_data_t *data;
} shared_memory_t;

/*
 * Function that will sleep a thread for a given time in miliseconds
 * Multiplier can be used to slow down or speed up all sleeps that use this function
 * 		- 1 is original time, (multiplier > 1) = slower, (0 < multiplier < 1) = faster
 */
int miliSleep(long tms)
{
    float multiplier = 4;
    struct timespec ts;
    int ret;

    tms = tms * multiplier;
    if (tms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;

    do
    {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}
