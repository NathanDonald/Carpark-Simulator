#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "structs.h"

/*
    Define constants
*/
#define REGO_TABLE_SIZE 100
#define LEVELS 5
#define ENTRANCES 5
#define EXITS 5

#define SHARE_NAME "PARKING"
#define SHMSZ 2920

/*
    Define Structs
*/
typedef struct rego rego_t;

struct rego
{
    char *key;
    int value;
    rego_t *next;
};

typedef struct carPark
{
    char carRego[6];
} carPark_t;

typedef struct carmanagement
{
    carPark_t cars[CARPARKS_PER_LEVEL];
    pthread_mutex_t m;
    int actualCounter;
    int assignedCounter;
} carmanagement_t;

typedef struct htab htab_t;

struct htab
{
    rego_t **regos;
    size_t size;
};

typedef struct billing billing_t;

struct billing
{
    char rego[6];
    float bill;
    int32_t time;
};

// Variables
htab_t rego_table;

// Simple methods
void *process(void *ptr);
void displayUI(void);
void read_rego_to_table(void);
void test(void);
int miliSleep(long tms);
void *billing(void *ptr);
void reset_rego(char *rego);

bool get_shared_object(shared_memory_t *shm);
void *entranceManager(void *ptr);
void *exitManager(void *ptr);
int allocate_level();
void *entryBoomManager(void *ptr);
void *exitBoomManager(void *ptr);
void *lvlManager(void *ptr);

void remove_from_level(int lprLevel);
void display_levels();