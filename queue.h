#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMPTY_QUEUE "EMPTY-"
 
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
queue_t * createQueue(int maxElements, int enterance_number)
{
    /* Create a Queue */
    queue_t *Q;
    Q = (queue_t *)malloc(sizeof(queue_t));
    /* Initialise its properties */
    Q->elements = (char**)malloc(sizeof(char*)*maxElements);
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
    if(Q->size!=0)
    {
        Q->size--;
        Q->front++;
        /* As we fill elements in circular fashion */
        if(Q->front==Q->capacity)
        {
                Q->front=0;
        }
    }
    return;
}

char* front(queue_t *Q)
{
    if(Q->size!=0)
    {
        /* Return the element which is at the front*/
        return Q->elements[Q->front];
    }
    return EMPTY_QUEUE;
}

int Enqueue(queue_t *Q , char *element)
{

    /* If the Queue is full, we cannot push an element into it as there is no space for it.*/
    if(Q->size == Q->capacity)
    {
        return 1;
        //printf("Queue is Full\n");
    }
    else
    {
        Q->size++;
        Q->rear = Q->rear + 1;
        /* As we fill the queue in circular fashion */
        if(Q->rear == Q->capacity)
        {
                Q->rear = 0;
        }
        /* Insert the element in its rear side */ 


        Q->elements[Q->rear] = (char *) malloc((sizeof element + 1)* sizeof(char));

        strcpy(Q->elements[Q->rear], element);
    }
    return 0;
}