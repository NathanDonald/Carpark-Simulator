#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"

#define MAXIMUM_CARS_IN_QUEUE 20
#define PARKS_PER_LEVEL 20
#define MAXIMUM_PARKS PARKS_PER_LEVEL *LEVELS
#define STARTING_TEMPERATURE 25
#define FIREALARM_PROBABILITIY 20

int miliSleep(long tms);
bool create_shared_object(shared_memory_t *shm, const char *share_name);
void *carGenerator(void *ptr);
void *testManager1(void *ptr);
void *testManager2(void *ptr);
void *queueManager(void *ptr);
void *exitQueueManager(void *ptr);
void *generateTemperature(void *ptr);
void *simulateFireAlarm(void *ptr);
void *evacuateSign(void *ptr);
void *car(void *ptr);
void *alarmActive(void *ptr);
void *entryBoom(void *ptr);
void *exitBoom(void *ptr);

typedef struct car
{
    pthread_mutex_t m;
    pthread_cond_t c;
    int lvl;
    char rego[6];
    char used;
} car_t;

typedef struct free_space
{
    pthread_mutex_t m;
    int parks;
} free_space_t;

/*Queue has five properties.
capacity stands for the maximum number of elements Queue can hold.
  Size stands for the current size of the Queue and elements is the array of elements.
  front is the index of first element (the index at which we remove the element)
  rear is the index of last element (the index at which we insert the element) */
typedef struct queue
{
    pthread_mutex_t lockQueue;
    int enterance_number;
    int capacity;
    int size;
    int front;
    int rear;
    char **elements;
} queue_t;

/* crateQueue function takes argument the maximum number of elements the Queue can hold, creates
   a Queue according to it and returns a pointer to the Queue. */
queue_t *createQueue(int maxElements, int enterance_number)
{
    /* Create a Queue */
    queue_t *Q;
    Q = (queue_t *)malloc(sizeof(queue_t));
    /* Initialise its properties */
    Q->elements = (char **)malloc(sizeof(char *) * maxElements);
    Q->size = 0;
    Q->capacity = maxElements;
    Q->front = 0;
    Q->rear = -1;
    Q->enterance_number = enterance_number;
    /* Return the pointer */
    return Q;
}

void Dequeue(queue_t *Q)
{
    if (Q->size != 0)
    {
        Q->size--;
        Q->front++;
        /* As we fill elements in circular fashion */
        if (Q->front == Q->capacity)
        {
            Q->front = 0;
        }
    }
    return;
}

char *front(queue_t *Q)
{
    if (Q->size != 0)
    {
        /* Return the element which is at the front*/
        return Q->elements[Q->front];
    }
    return EMPTY_QUEUE;
}

int Enqueue(queue_t *Q, char *element)
{

    /* If the Queue is full, we cannot push an element into it as there is no space for it.*/
    if (Q->size == Q->capacity)
    {
        return 1;
        // printf("Queue is Full\n");
    }
    else
    {
        Q->size++;
        Q->rear = Q->rear + 1;
        /* As we fill the queue in circular fashion */
        if (Q->rear == Q->capacity)
        {
            Q->rear = 0;
        }
        /* Insert the element in its rear side */

        Q->elements[Q->rear] = (char *)malloc((sizeof element + 1) * sizeof(char));

        strcpy(Q->elements[Q->rear], element);
    }
    return 0;
}

bool get_shared_object(shared_memory_t *shm, const char *share_name)
{
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd. If the operation fails, ensure that shm->data is
    // NULL and return false.
    if ((shm->fd = shm_open(share_name, O_RDWR, 0)) < 0)
    {
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    if ((shm->data = mmap(0, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (shared_data_t *)-1)
    {
        return false;
    }

    // Modify the remaining stub only if necessary.
    return true;
}