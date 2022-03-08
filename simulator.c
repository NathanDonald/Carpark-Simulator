#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

#include "simulator.h"

queue_t *Qs[ENTRANCES];
queue_t *ExitQs[EXITS];
queue_t *acceptedPlatesQueue;

boomgate_t entryBoomObj[ENTRANCES];
boomgate_t exitBoomObj[EXITS];

car_t car_Object[MAXIMUM_PARKS];
int number_of_cars = 0;
free_space_t free_space_lvl[LEVELS];

// Create the shared memory
shared_memory_t shm;

bool run = true;

/*
 * Lock rand() to be thread safe
 */

pthread_mutex_t lockRand;

int main(void)
{
    // Get the number plates from the file
    acceptedPlatesQueue = createQueue(101, 0);
    // Create the 5 queues

    for (int i = 0; i < ENTRANCES; ++i)
    {
        Qs[i] = createQueue(MAXIMUM_CARS_IN_QUEUE, (i + 1));
    }

    for (int i = 0; i < EXITS; ++i)
    {

        ExitQs[i] = createQueue(MAXIMUM_CARS_IN_QUEUE, (i + 1));
    }

    if (create_shared_object(&shm, SHARE_NAME))
    {
        // Make Mutex and Condition variables publics
        pthread_mutexattr_t mutexattr;
        pthread_mutexattr_init(&mutexattr);

        pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);

        pthread_condattr_t condattr;
        pthread_condattr_init(&condattr);

        pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);

        for (int i = 0; i < ENTRANCES; i++)
        {
            //-------------------Entry-------------------
            // Parking sign
            pthread_mutex_init(&shm.data->entries[i].parkingsign.m, &mutexattr);
            pthread_cond_init(&shm.data->entries[i].parkingsign.c, &condattr);
            // Boom gate
            pthread_mutex_init(&shm.data->entries[i].boomgate.m, &mutexattr);
            pthread_cond_init(&shm.data->entries[i].boomgate.c, &condattr);
            pthread_mutex_init(&entryBoomObj[i].m, &mutexattr);
            pthread_cond_init(&entryBoomObj[i].c, &condattr);
            // LPR
            pthread_mutex_init(&shm.data->entries[i].lpr.m, &mutexattr);
            pthread_cond_init(&shm.data->entries[i].lpr.c, &condattr);
        }
        for (int i = 0; i < LEVELS; i++)
        {
            //-------------------Level-------------------
            // LPR
            pthread_mutex_init(&shm.data->levels[i].lpr.m, &mutexattr);
            pthread_cond_init(&shm.data->levels[i].lpr.c, &condattr);

            // Parking spots for each level
            pthread_mutex_init(&free_space_lvl[i].m, &mutexattr);
            free_space_lvl[i].parks = PARKS_PER_LEVEL;
        }
        for (int i = 0; i < EXITS; i++)
        {
            //-------------------Exit-------------------
            // Boom gate
            pthread_mutex_init(&shm.data->exits[i].boomgate.m, &mutexattr);
            pthread_cond_init(&shm.data->exits[i].boomgate.c, &condattr);
            // LPR
            pthread_mutex_init(&shm.data->exits[i].lpr.m, &mutexattr);
            pthread_cond_init(&shm.data->exits[i].lpr.c, &condattr);
        }

        for (int f = 0; f < MAXIMUM_PARKS; f++)
        {

            pthread_mutex_init(&car_Object[f].m, &mutexattr);
            pthread_cond_init(&car_Object[f].c, &condattr);
        }

        pthread_mutex_lock(&shm.data->entries[0].parkingsign.m);
        pthread_cond_wait(&shm.data->entries[0].parkingsign.c, &shm.data->entries[0].parkingsign.m);
        pthread_mutex_unlock(&shm.data->entries[0].parkingsign.m);
        miliSleep(5);

        // Spawn Threads
        pthread_t carGeneratorThread;
        pthread_t testManager1Thread;
        pthread_t testManager2Thread;
        pthread_t queueManagerThread[ENTRANCES];
        pthread_t exitQueueManagerThread[EXITS];
        pthread_t carThread[MAXIMUM_PARKS];
        pthread_t simulateFireAlarmThread;
        pthread_t evacuateSignThread;
        pthread_t entryBoomThread[ENTRANCES];
        pthread_t exitBoomThread[EXITS];

        pthread_create(&carGeneratorThread, NULL, carGenerator, NULL);
        pthread_create(&testManager1Thread, NULL, testManager1, NULL);
        pthread_create(&testManager2Thread, NULL, testManager2, NULL);
        pthread_create(&simulateFireAlarmThread, NULL, simulateFireAlarm, NULL);
        pthread_create(&evacuateSignThread, NULL, evacuateSign, NULL);

        for (int xyz = 0; xyz < ENTRANCES; ++xyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = xyz;
            pthread_create(&queueManagerThread[*loop_number], NULL, queueManager, (void *)loop_number);
            pthread_create(&entryBoomThread[*loop_number], NULL, entryBoom, (void *)loop_number);
        }

        for (int s = 0; s < EXITS; ++s)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = s;
            pthread_create(&exitQueueManagerThread[*loop_number], NULL, exitQueueManager, (void *)loop_number);
            pthread_create(&exitBoomThread[*loop_number], NULL, exitBoom, (void *)loop_number);
        }

        for (int xy = 0; xy < MAXIMUM_PARKS; ++xy)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = xy;
            pthread_create(&carThread[*loop_number], NULL, car, (void *)loop_number);
        }

        // Wait for thread to finish
        pthread_join(carGeneratorThread, NULL);
        pthread_join(testManager1Thread, NULL);
        pthread_join(testManager2Thread, NULL);
        pthread_join(simulateFireAlarmThread, NULL);
        pthread_join(evacuateSignThread, NULL);

        for (int wxyz = 0; wxyz < ENTRANCES; ++wxyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = wxyz;
            pthread_join(queueManagerThread[*loop_number], NULL);
            pthread_join(entryBoomThread[*loop_number], NULL);
        }

        for (int swxyz = 0; swxyz < EXITS; ++swxyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = swxyz;
            pthread_join(exitQueueManagerThread[*loop_number], NULL);
            pthread_join(exitBoomThread[*loop_number], NULL);
        }

        for (int x = 0; x < MAXIMUM_PARKS; ++x)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = x;
            pthread_join(carThread[*loop_number], NULL);
        }
    }
    else
    {
        printf("Shared memory creation failed.\n");
    }

    return 1;
}

/*
 * Generate a random car license plate
 *
 *  TODO: check that we are not creating a plate that is already in the car park.
 *
 */
char *createRego()
{

    char *rego = malloc(6);

    for (int i = 0; i < 3; i++)
    {
        pthread_mutex_lock(&lockRand);

        rego[i + 3] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26];
        rego[i] = "0123456789"[rand() % 10];

        pthread_mutex_unlock(&lockRand);
    }

    return rego;
}

void *evacuateSign(void *ptr)
{
    // bool evacuate = true;
    // while (evacuate)
    // {
    //     miliSleep(20);
    //     // printf("%d\n", shm.data->levels[0].alarm);
    //     if (shm.data->levels[0].alarm == 1)
    //     {
    //         evacuate = false;
    //         printf("\n");
    //         // printf("*** ALARM ACTIVE ***\n");
    //         for (int i = 0; i < 9; ++i)
    //         {
    //             printf("%c", shm.data->entries[0].parkingsign.display);
    //             miliSleep(20);
    //         }
    //         printf("\n");
    //     }
    // }
    return ptr;
}

void *simulateFireAlarm(void *ptr)
{
    int sleep_random;
    int temp_random;
    int temperature[LEVELS];
    for (int i = 0; i < LEVELS; i++)
        temperature[i] = STARTING_TEMPERATURE;
    int old_temp[LEVELS];

    while (1)
    {
        pthread_mutex_lock(&lockRand);
        sleep_random = rand() % 5;
        pthread_mutex_unlock(&lockRand);
        miliSleep((long)sleep_random);
        for (int i = 0; i < LEVELS; i++)
        {
            pthread_mutex_lock(&lockRand);
            temp_random = rand() % FIREALARM_PROBABILITIY;
            pthread_mutex_unlock(&lockRand);
            if (old_temp[i] == 1)
            {
                if (temp_random < 2)
                {
                    temperature[i]++;
                    old_temp[i] = 1;
                }
                else
                {
                    temperature[i]--;
                    old_temp[i] = 0;
                }
            }
            else
            {
                if (temp_random >= 2)
                {
                    temperature[i]++;
                    old_temp[i] = 1;
                }
                else
                {
                    temperature[i]--;
                    old_temp[i] = 0;
                }
            }
            shm.data->levels[i].tempnode.temperature = temperature[i];
            // printf("%d level; alarm: %d; temp: %d\n", i, shm.data->levels[i].alarm, shm.data->levels[i].tempnode.temperature);
        }
    }

    return ptr;
}

/*
 * Generate a car with a random license plate every 1-100 ms
 */
void *carGenerator(void *ptr)
{

    char *fileName = "plates.txt";

    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(fileName, "r");
    if (fp == NULL)
        printf("fail\n");

    while ((read = getline(&line, &len, fp)) != -1)
    {
        line[strcspn(line, "\n")] = 0;
        pthread_mutex_lock(&acceptedPlatesQueue->lockQueue);
        Enqueue(acceptedPlatesQueue, line);
        pthread_mutex_unlock(&acceptedPlatesQueue->lockQueue);
    }

    fclose(fp);
    if (line)
        free(line);

    // Set variables that will be used in the run loop
    srand(time(NULL));
    int x = 0;
    bool acceptedPlate;
    int fullQueue;

    pthread_mutex_init(&lockRand, NULL);

    // Make Mutex and Condition variables publics
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);

    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&lockRand, &mutexattr);

    pthread_mutex_init(&acceptedPlatesQueue->lockQueue, &mutexattr);

    for (int i = 0; i < ENTRANCES; ++i)
        pthread_mutex_init(&Qs[i]->lockQueue, &mutexattr);

    while (run && shm.data->levels[0].alarm == 0)
    {
        pthread_mutex_lock(&lockRand);

        // printf("%d\n", x);

        int random_number = rand() % 99 + 1;
        int plateOrRandom = rand() % 3;
        int enteranceNumber = rand() % ENTRANCES;

        pthread_mutex_unlock(&lockRand);

        miliSleep(random_number);

        char *carRego;
        carRego = createRego();

        // Check if plate will be random or one that will be accepted
        if (plateOrRandom == 2)
        {
            pthread_mutex_lock(&acceptedPlatesQueue->lockQueue);
            carRego = front(acceptedPlatesQueue);
            Dequeue(acceptedPlatesQueue);
            pthread_mutex_unlock(&acceptedPlatesQueue->lockQueue);
            acceptedPlate = true;
            // printf("Added accepted car\n");
        }
        else
        {
            carRego = createRego();
            acceptedPlate = false;
            // printf("Added random car\n");
        }

        pthread_mutex_lock(&Qs[enteranceNumber]->lockQueue);
        fullQueue = Enqueue(Qs[enteranceNumber], carRego);
        pthread_mutex_unlock(&Qs[enteranceNumber]->lockQueue);

        // If the queue was full then the car turned away - If the car was from the text file it needs to be re added to the queue
        if ((fullQueue == 1) && acceptedPlate)
        {
            pthread_mutex_lock(&acceptedPlatesQueue->lockQueue);
            Enqueue(acceptedPlatesQueue, carRego);
            pthread_mutex_unlock(&acceptedPlatesQueue->lockQueue);
            // printf("Queue was full so returned accepted car to queue\n");
        }

        // Break after 50 cars have been created to stop program
        x++;
        if (x > 300)
        {
            run = false;
        }
    }

    return ptr;
}

void *testManager1(void *ptr)
{
    // bool run = true;
    miliSleep(200);

    // shared_memory_t shm;
    // if (get_shared_object(&shm, SHARE_NAME))
    // {
    //     pthread_mutex_lock(&shm.data->entries[1].parkingsign.m);
    //     shm.data->entries[1].parkingsign.display = '1';
    //     pthread_cond_signal(&shm.data->entries[1].parkingsign.c);
    //     pthread_mutex_unlock(&shm.data->entries[1].parkingsign.m);

    //     miliSleep(700);
    //     pthread_mutex_lock(&shm.data->entries[1].boomgate.m);
    //     shm.data->entries[1].boomgate.s = 'O';
    //     printf("signaling boomgate 1\n");
    //     pthread_cond_signal(&shm.data->entries[1].boomgate.c);
    //     pthread_mutex_unlock(&shm.data->entries[1].boomgate.m);

    //     miliSleep(100);
    //     pthread_mutex_lock(&shm.data->entries[2].boomgate.m);
    //     shm.data->entries[2].boomgate.s = 'O';
    //     printf("signaling boomgate 2\n");
    //     pthread_cond_signal(&shm.data->entries[2].boomgate.c);
    //     pthread_mutex_unlock(&shm.data->entries[2].boomgate.m);

    //     miliSleep(1400);
    //     pthread_mutex_lock(&shm.data->exits[2].boomgate.m);
    //     shm.data->exits[2].boomgate.s = 'O';
    //     printf("signaling Exit boomgate 2\n");
    //     pthread_cond_signal(&shm.data->exits[2].boomgate.c);
    //     pthread_mutex_unlock(&shm.data->exits[2].boomgate.m);

    //     pthread_mutex_lock(&shm.data->exits[1].boomgate.m);
    //     shm.data->exits[2].boomgate.s = 'O';
    //     printf("signaling Exit boomgate 1\n");
    //     pthread_cond_signal(&shm.data->exits[1].boomgate.c);
    //     pthread_mutex_unlock(&shm.data->exits[1].boomgate.m);

    // }
    // else
    // {
    //     printf("Shared memory connection failed.\n");
    // }

    return ptr;
}

void *testManager2(void *ptr)
{
    miliSleep(300);

    // shared_memory_t shm;
    // if (get_shared_object(&shm, SHARE_NAME))
    // {
    //     pthread_mutex_lock(&shm.data->entries[2].parkingsign.m);
    //     shm.data->entries[2].parkingsign.display = '4';
    //     pthread_cond_signal(&shm.data->entries[2].parkingsign.c);
    //     pthread_mutex_unlock(&shm.data->entries[2].parkingsign.m);
    // }
    // else
    // {
    //     printf("Shared memory connection failed.\n");
    // }
    return ptr;
}

int assignCar(char *rego, int lvl)
{
    for (int i = 0; i < (MAXIMUM_PARKS); i++)
    {
        pthread_mutex_lock(&car_Object[i].m);
        if (car_Object[i].used == 'N')
        {
            car_Object[i].used = 'Y';
            car_Object[i].lvl = lvl;
            strcpy(car_Object[i].rego, rego);
            pthread_cond_signal(&car_Object[i].c);
            pthread_mutex_unlock(&car_Object[i].m);
            printf("car %s assigned to %d going to level %d\n", rego, i, lvl);
            return 1;
        }
        pthread_mutex_unlock(&car_Object[i].m);
    }
    return 0;
}

void *queueManager(void *ptr)
{
    int queue_number = *((int *)ptr);

    shm.data->entries[queue_number].parkingsign.display = '-';

    while (run && shm.data->levels[0].alarm == 0)
    {

        pthread_mutex_lock(&Qs[queue_number]->lockQueue);
        char *frontRego = front(Qs[queue_number]);
        pthread_mutex_unlock(&Qs[queue_number]->lockQueue);

        if (strcmp(frontRego, EMPTY_QUEUE))
        {

            // Wait 2 miliseconds befor triggering the lpr
            miliSleep(2);
            // printf("%s\n", frontRego);

            // Add the plate to the LPR
            pthread_mutex_lock(&shm.data->entries[queue_number].lpr.m);

            strcpy(shm.data->entries[queue_number].lpr.rego, frontRego);
            // Signal and unlock the LPR
            printf("signaling lpr %d\n", queue_number);
            pthread_cond_signal(&shm.data->entries[queue_number].lpr.c);
            pthread_mutex_unlock(&shm.data->entries[queue_number].lpr.m);

            // Dequeue the plate from the queue
            pthread_mutex_lock(&Qs[queue_number]->lockQueue);
            Dequeue(Qs[queue_number]);
            pthread_mutex_unlock(&Qs[queue_number]->lockQueue);

            // Wait for the parkingsign to give a level
            pthread_mutex_lock(&shm.data->entries[queue_number].parkingsign.m);

            while (shm.data->entries[queue_number].parkingsign.display == '-')
            {
                printf("waiting for parking sign %d\n", queue_number);
                pthread_cond_wait(&shm.data->entries[queue_number].parkingsign.c, &shm.data->entries[queue_number].parkingsign.m); // wait for the condition
            }
            char parkingDisplay = shm.data->entries[queue_number].parkingsign.display;

            pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);

            if (parkingDisplay == 'X')
            {
                printf("Car %s was turned away\n", frontRego);
            }
            else
            {
                if (shm.data->exits[queue_number].boomgate.s != 'O')
                {
                    pthread_mutex_lock(&entryBoomObj[queue_number].m);
                    while (shm.data->entries[queue_number].boomgate.s != 'O')
                    {
                        printf("car %s is waiting to enter at entry %d\n", frontRego, queue_number);
                        pthread_cond_wait(&entryBoomObj[queue_number].c, &entryBoomObj[queue_number].m); // wait for the condition
                    }
                    pthread_mutex_unlock(&entryBoomObj[queue_number].m);
                }

                if (number_of_cars >= MAXIMUM_PARKS)
                {
                    printf("car turned away as car park was full\n");
                }
                else
                {
                    number_of_cars++;

                    if (assignCar(frontRego, shm.data->entries[queue_number].parkingsign.display))
                        printf("car %s pushed to level %d from enterance %d\n",
                               frontRego, shm.data->entries[queue_number].parkingsign.display, queue_number);
                    else
                        printf("Error - No cars left\n");
                }
            }

            pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);
            shm.data->entries[queue_number].parkingsign.display = '-';
            pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);
        }
    }

    return ptr;
}

/*
 * Car Threads
 */

void *car(void *ptr)
{
    int car_number = *((int *)ptr);

    pthread_mutex_lock(&car_Object[car_number].m);
    strcpy(car_Object[car_number].rego, EMPTY_LPR);
    car_Object[car_number].used = 'N';
    pthread_mutex_unlock(&car_Object[car_number].m);

    while (run)
    {
        pthread_mutex_lock(&car_Object[car_number].m);
        while (car_Object[car_number].used == 'N')
        {
            pthread_cond_wait(&car_Object[car_number].c, &car_Object[car_number].m); // wait for the condition
        }

        pthread_mutex_unlock(&car_Object[car_number].m);

        // Time to drive to the car park
        miliSleep(10);

        pthread_mutex_lock(&car_Object[car_number].m);
        // TRIGGER THE LPR FOR THE LEVEL
        pthread_mutex_lock(&shm.data->levels[car_Object[car_number].lvl].lpr.m);
        strcpy(shm.data->levels[car_Object[car_number].lvl].lpr.rego, car_Object[car_number].rego);
        pthread_cond_signal(&shm.data->levels[car_Object[car_number].lvl].lpr.c);
        pthread_mutex_unlock(&shm.data->levels[car_Object[car_number].lvl].lpr.m);

        // Park in a space if there is one free
        pthread_mutex_lock(&free_space_lvl[car_Object[car_number].lvl].m);
        if (free_space_lvl[car_Object[car_number].lvl].parks >= 0)
        {
            free_space_lvl[car_Object[car_number].lvl].parks--;
        }
        else
        {
            printf("level %d is full and could not fit the car\n", car_Object[car_number].lvl);
        }
        pthread_mutex_unlock(&free_space_lvl[car_Object[car_number].lvl].m);
        printf("car %d is being used by %s going to level %d an there are now %d spaces left\n", car_number, car_Object[car_number].rego, car_Object[car_number].lvl, free_space_lvl[car_Object[car_number].lvl].parks);

        pthread_mutex_unlock(&car_Object[car_number].m);

        // Sleep the car while it is parked for a random amount of time between 100 and 10000ms
        pthread_mutex_lock(&lockRand);

        int random_time = rand() % 9900 + 100;
        int exitNumber = rand() % EXITS;

        pthread_mutex_unlock(&lockRand);

        miliSleep(random_time);

        pthread_mutex_lock(&car_Object[car_number].m);

        // Free the space as the car leaves and heads to exit
        pthread_mutex_lock(&free_space_lvl[car_Object[car_number].lvl].m);
        free_space_lvl[car_Object[car_number].lvl].parks++;
        pthread_mutex_unlock(&free_space_lvl[car_Object[car_number].lvl].m);

        // TRIGGER THE LPR FOR THE LEVEL
        pthread_mutex_lock(&shm.data->levels[car_Object[car_number].lvl].lpr.m);
        strcpy(shm.data->levels[car_Object[car_number].lvl].lpr.rego, car_Object[car_number].rego);
        pthread_cond_signal(&shm.data->levels[car_Object[car_number].lvl].lpr.c);
        pthread_mutex_unlock(&shm.data->levels[car_Object[car_number].lvl].lpr.m);

        pthread_mutex_unlock(&car_Object[car_number].m);
        // Takes 10 ms for the car to drive to the exit
        miliSleep(10);
        pthread_mutex_lock(&car_Object[car_number].m);

        bool foundQueue = false;
        while (!foundQueue)
        {
            pthread_mutex_lock(&ExitQs[exitNumber]->lockQueue);
            // Attempt to queue in the random exit picked and if it is full look for an empty one
            int fullQueue = Enqueue(ExitQs[exitNumber], car_Object[car_number].rego);
            if (fullQueue)
            {
                printf("queue was full\n");
                if (exitNumber < (EXITS - 1))
                {
                    exitNumber++;
                }
                else
                {
                    exitNumber = 0;
                }
            }
            else
            {
                foundQueue = true;
            }
            pthread_mutex_unlock(&ExitQs[exitNumber]->lockQueue);
        }

        printf("car %d is being used by %s leaving level %d an there are now %d spaces left\n", car_number, car_Object[car_number].rego, car_Object[car_number].lvl, free_space_lvl[car_Object[car_number].lvl].parks);

        pthread_mutex_lock(&acceptedPlatesQueue->lockQueue);
        Enqueue(acceptedPlatesQueue, car_Object[car_number].rego);
        pthread_mutex_unlock(&acceptedPlatesQueue->lockQueue);

        // Empty the car object and unlock the mutex
        car_Object[car_number].used = 'N';
        pthread_mutex_unlock(&car_Object[car_number].m);
        number_of_cars--;
    }

    return ptr;
}

/*
 * Queues for the exits
 */
void *exitQueueManager(void *ptr)
{

    int queue_number = *((int *)ptr);

    while (run)
    {
        pthread_mutex_lock(&ExitQs[queue_number]->lockQueue);
        char *frontRego = front(ExitQs[queue_number]);
        pthread_mutex_unlock(&ExitQs[queue_number]->lockQueue);

        if (strcmp(frontRego, EMPTY_QUEUE))
        {
            // Wait 2 miliseconds befor triggering the lpr
            miliSleep(2);

            // Add the plate to the LPR
            pthread_mutex_lock(&shm.data->exits[queue_number].lpr.m);

            strcpy(shm.data->exits[queue_number].lpr.rego, frontRego);
            // Signal and unlock the LPR
            pthread_cond_signal(&shm.data->exits[queue_number].lpr.c);
            pthread_mutex_unlock(&shm.data->exits[queue_number].lpr.m);

            // Dequeue the plate from the queue
            pthread_mutex_lock(&ExitQs[queue_number]->lockQueue);
            Dequeue(ExitQs[queue_number]);
            pthread_mutex_unlock(&ExitQs[queue_number]->lockQueue);

            // Wait for the boomgate to open
            if (shm.data->exits[queue_number].boomgate.s != 'O')
            {
                pthread_mutex_lock(&exitBoomObj[queue_number].m);
                while (shm.data->exits[queue_number].boomgate.s != 'O')
                {
                    printf("car %s is waiting to leave at exit %d\n", frontRego, queue_number);
                    pthread_cond_wait(&exitBoomObj[queue_number].c, &exitBoomObj[queue_number].m); // wait for the condition
                }
                pthread_mutex_unlock(&exitBoomObj[queue_number].m);
            }

            // Empty the LPR
            pthread_mutex_lock(&shm.data->exits[queue_number].lpr.m);
            strcpy(shm.data->exits[queue_number].lpr.rego, EMPTY_LPR);
            pthread_mutex_unlock(&shm.data->exits[queue_number].lpr.m);
        }
    }

    return ptr;
}

void *entryBoom(void *ptr)
{
    int entry_number = *((int *)ptr);

    // Ensure gates are closed on start
    pthread_mutex_lock(&shm.data->entries[entry_number].boomgate.m);
    shm.data->entries[entry_number].boomgate.s = 'C';
    pthread_mutex_unlock(&shm.data->entries[entry_number].boomgate.m);

    while (run)
    {
        pthread_mutex_lock(&shm.data->entries[entry_number].boomgate.m);

        pthread_cond_wait(&shm.data->entries[entry_number].boomgate.c, &shm.data->entries[entry_number].boomgate.m);

        char boomState = shm.data->entries[entry_number].boomgate.s;
        pthread_mutex_unlock(&shm.data->entries[entry_number].boomgate.m);

        // printf("boom woke with state %c\n", boomState);

        switch (boomState)
        {
        case 'L':
            // printf("Gate %d Closing\n", entry_number);
            // //miliSleep(10);

            // pthread_mutex_lock(&shm.data->entries[entry_number].boomgate.m);
            // shm.data->entries[entry_number].boomgate.s = 'C';
            // pthread_cond_signal(&shm.data->entries[entry_number].boomgate.c);
            // pthread_mutex_unlock(&shm.data->entries[entry_number].boomgate.m);

            // break;
        case 'R':

            // miliSleep(10);

            pthread_mutex_lock(&shm.data->entries[entry_number].boomgate.m);
            shm.data->entries[entry_number].boomgate.s = 'O';
            pthread_cond_signal(&shm.data->entries[entry_number].boomgate.c);
            pthread_mutex_unlock(&shm.data->entries[entry_number].boomgate.m);
            printf("Gate %d open\n", entry_number);

            pthread_mutex_lock(&entryBoomObj[entry_number].m);
            pthread_cond_signal(&entryBoomObj[entry_number].c);
            pthread_mutex_unlock(&entryBoomObj[entry_number].m);
            break;
        default:
            printf("Boomgate state not known\n");
            break;
        }
    }

    return ptr;
}
void *exitBoom(void *ptr)
{
    int entry_number = *((int *)ptr);

    // Ensure gates are closed on start
    pthread_mutex_lock(&shm.data->exits[entry_number].boomgate.m);
    shm.data->exits[entry_number].boomgate.s = 'C';
    pthread_mutex_unlock(&shm.data->exits[entry_number].boomgate.m);

    while (run)
    {
        pthread_mutex_lock(&shm.data->exits[entry_number].boomgate.m);

        pthread_cond_wait(&shm.data->exits[entry_number].boomgate.c, &shm.data->exits[entry_number].boomgate.m);

        char boomState = shm.data->exits[entry_number].boomgate.s;
        pthread_mutex_unlock(&shm.data->exits[entry_number].boomgate.m);

        // printf("boom woke with state %c\n", boomState);

        switch (boomState)
        {
        case 'L':
            // printf("Exit Gate %d Closing\n", entry_number);
            // //miliSleep(10);

            // pthread_mutex_lock(&shm.data->exits[entry_number].boomgate.m);
            // shm.data->exits[entry_number].boomgate.s = 'C';
            // pthread_cond_signal(&shm.data->exits[entry_number].boomgate.c);
            // pthread_mutex_unlock(&shm.data->exits[entry_number].boomgate.m);

            // break;
        case 'R':

            // miliSleep(10);

            pthread_mutex_lock(&shm.data->exits[entry_number].boomgate.m);
            shm.data->exits[entry_number].boomgate.s = 'O';
            pthread_cond_signal(&shm.data->exits[entry_number].boomgate.c);
            pthread_mutex_unlock(&shm.data->exits[entry_number].boomgate.m);
            printf("Exit Gate %d open\n", entry_number);

            pthread_mutex_lock(&exitBoomObj[entry_number].m);
            pthread_cond_signal(&exitBoomObj[entry_number].c);
            pthread_mutex_unlock(&exitBoomObj[entry_number].m);
            break;
        default:
            printf("Boomgate state not known\n");
            break;
        }
    }

    return ptr;
}

/*
 * Shared Memory
 */
bool create_shared_object(shared_memory_t *shm, const char *share_name)
{
    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(share_name);

    // Assign share name to shm->name.
    shm->name = share_name;

    // Create the shared memory object, allowing read-write access, and saving the
    // resulting file descriptor in shm->fd. If creation failed, ensure
    // that shm->data is NULL and return false.
    if ((shm->fd = shm_open(shm->name, O_CREAT | O_RDWR, 0666)) < 0)
    {
        shm->data = NULL;
        return false;
    }

    // Set the capacity of the shared memory object via ftruncate. If the
    // operation fails, ensure that shm->data is NULL and return false.
    if (ftruncate(shm->fd, sizeof(shared_data_t)) == -1)
    {
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    // struct shared_data *data;

    if ((shm->data = mmap(0, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0)) == (shared_data_t *)-1)
    {
        return false;
    }

    // If we reach this point we should return true.
    return true;
}