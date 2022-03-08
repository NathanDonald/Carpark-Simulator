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

#include "manager.h"
#include "regotable.c"

carmanagement_t car_per_lvl[LEVELS];
double totalRevenue;
/*
    For billing, the rego table will load all the regos into the hashtable
    the value will then be incremented every X and will be held as the timer
    for how long the vehicle has been staying
    If value == -1 then theres no vehicle in area
    any value above that and theres a vehicle
*/

// Create the shared memory
shared_memory_t shm;
bool run = true;

boomgate_t entryBoomObjm[ENTRANCES];
boomgate_t exitBoomObjm[EXITS];

int main(void)
{

    // Initialize rego table
    if (!htab_init(&rego_table, REGO_TABLE_SIZE))
    {
        // printf("Error initializing hash table\n");
        return EXIT_FAILURE;
    }

    read_rego_to_table();
    // 1ms thread for 5c every 1ms

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

        pthread_mutex_init(&entryBoomObjm[i].m, &mutexattr);
        pthread_cond_init(&entryBoomObjm[i].c, &condattr);
    }
    for (int i = 0; i < EXITS; i++)
    {
        //-------------------Entry-------------------

        pthread_mutex_init(&exitBoomObjm[i].m, &mutexattr);
        pthread_cond_init(&exitBoomObjm[i].c, &condattr);
    }
    for (int i = 0; i < LEVELS; i++)
    {
        //-------------------Level-------------------

        pthread_mutex_init(&car_per_lvl[i].m, &mutexattr);
    }

    shm.name = SHARE_NAME;

    if (get_shared_object(&shm))
    {

        pthread_mutex_lock(&shm.data->entries[0].parkingsign.m);
        pthread_cond_signal(&shm.data->entries[0].parkingsign.c);
        pthread_mutex_unlock(&shm.data->entries[0].parkingsign.m);
        // Spawn Threads
        pthread_t entranceManagerThread[ENTRANCES];
        pthread_t exitManagerThread[EXITS];
        pthread_t entryBoomManagerThread[ENTRANCES];
        pthread_t exitBoomManagerThread[EXITS];
        pthread_t lvlManagerThread[LEVELS];
        pthread_t billingThread;
        pthread_t processThread;
        pthread_create(&billingThread, NULL, billing, NULL);
        pthread_create(&processThread, NULL, process, NULL);

        for (int xyz = 0; xyz < ENTRANCES; ++xyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = xyz;
            pthread_create(&entranceManagerThread[*loop_number], NULL, entranceManager, (void *)loop_number);
            pthread_create(&entryBoomManagerThread[*loop_number], NULL, entryBoomManager, (void *)loop_number);
        }

        for (int xyzs = 0; xyzs < LEVELS; ++xyzs)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = xyzs;
            pthread_create(&lvlManagerThread[*loop_number], NULL, lvlManager, (void *)loop_number);
        }

        for (int s = 0; s < EXITS; ++s)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = s;
            pthread_create(&exitManagerThread[*loop_number], NULL, exitManager, (void *)loop_number);
            pthread_create(&exitBoomManagerThread[*loop_number], NULL, exitBoomManager, (void *)loop_number);
        }

        for (int wxyz = 0; wxyz < ENTRANCES; ++wxyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = wxyz;
            pthread_join(entranceManagerThread[*loop_number], NULL);
            pthread_join(entryBoomManagerThread[*loop_number], NULL);
        }
        for (int wxyzt = 0; wxyzt < LEVELS; ++wxyzt)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = wxyzt;
            pthread_join(lvlManagerThread[*loop_number], NULL);
        }

        for (int swxyz = 0; swxyz < EXITS; ++swxyz)
        {
            int *loop_number = malloc(sizeof(int));
            *loop_number = swxyz;
            pthread_join(exitManagerThread[*loop_number], NULL);
            pthread_join(exitBoomManagerThread[*loop_number], NULL);
        }
        pthread_join(billingThread, NULL);
        pthread_join(processThread, NULL);

        pthread_mutexattr_t mutexattr;
        pthread_mutexattr_init(&mutexattr);

        pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);

        pthread_condattr_t condattr;
        pthread_condattr_init(&condattr);

        pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);

        for (int i = 0; i < ENTRANCES; i++)
        {
            //-------------------Entry-------------------
            // Boom gate
            pthread_mutex_init(&shm.data->entries[i].boomgate.m, &mutexattr);
            pthread_cond_init(&shm.data->entries[i].boomgate.c, &condattr);
        }

        for (int i = 0; i < EXITS; i++)
        {
            //-------------------Exit-------------------
            // Boom gate
            pthread_mutex_init(&shm.data->entries[i].boomgate.m, &mutexattr);
            pthread_cond_init(&shm.data->entries[i].boomgate.c, &condattr);
        }
    }
    else
    {
        printf("Shared memory creation failed.\n");
    }

    return 1;
}

void *entranceManager(void *ptr)
{
    int queue_number = *((int *)ptr);

    // Make sure LPR is Empty to start
    pthread_mutex_lock(&shm.data->entries[queue_number].lpr.m);
    strcpy(shm.data->entries[queue_number].lpr.rego, EMPTY_LPR);
    pthread_mutex_unlock(&shm.data->entries[queue_number].lpr.m);

    for (int i = 0; i < LEVELS; i++)
    {
        // printf("%d lvl\n", i);
        // for(int s = 0; s<CARPARKS_PER_LEVEL; s++){

        pthread_mutex_lock(&car_per_lvl[i].m);
        // printf("%d space\n", s);
        car_per_lvl[i].actualCounter = 0;
        car_per_lvl[i].assignedCounter = 0;
        // car_per_lvl[i].cars[s] = EMPTY_LPR;
        // printf("%d space after edit\n", s);
        pthread_mutex_unlock(&car_per_lvl[i].m);

        //}
    }

    while (run)
    {

        // Wait for a plate to be in the LPR
        pthread_mutex_lock(&shm.data->entries[queue_number].lpr.m);
        while (strcmp(shm.data->entries[queue_number].lpr.rego, EMPTY_LPR) == 0)
        {
            // printf("waiting for lpr %d\n", queue_number);
            pthread_cond_wait(&shm.data->entries[queue_number].lpr.c, &shm.data->entries[queue_number].lpr.m);
        }
        char *plate = shm.data->entries[queue_number].lpr.rego;
        pthread_mutex_unlock(&shm.data->entries[queue_number].lpr.m);

        // printf("car with plate %s triggered lpr at entrance %d\n", plate, queue_number);

        if (htab_find(&rego_table, plate))
        { // Found rego
            // printf("car in queue %d accepted\n", queue_number);
            set_rego_value(&rego_table, htab_find(&rego_table, plate), 0);
            int givenLevel = allocate_level();

            if (givenLevel != -1)
            { // If it found a level
                pthread_mutex_lock(&shm.data->entries[queue_number].parkingsign.m);

                shm.data->entries[queue_number].parkingsign.display = givenLevel;
                pthread_cond_signal(&shm.data->entries[queue_number].parkingsign.c);
                pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);

                if (shm.data->entries[queue_number].boomgate.s != 'O')
                {

                    pthread_mutex_lock(&entryBoomObjm[queue_number].m);
                    pthread_cond_signal(&entryBoomObjm[queue_number].c);
                    pthread_mutex_unlock(&entryBoomObjm[queue_number].m);
                }
            }
            else
            { // Cant find a level
                pthread_mutex_lock(&shm.data->entries[queue_number].parkingsign.m);

                shm.data->entries[queue_number].parkingsign.display = 'X';
                pthread_cond_signal(&shm.data->entries[queue_number].parkingsign.c);
                pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);
            }
        }
        else
        {

            pthread_mutex_lock(&shm.data->entries[queue_number].parkingsign.m);

            shm.data->entries[queue_number].parkingsign.display = 'X';
            pthread_cond_signal(&shm.data->entries[queue_number].parkingsign.c);
            pthread_mutex_unlock(&shm.data->entries[queue_number].parkingsign.m);
        }

        // Make sure LPR is Empty to end

        pthread_mutex_lock(&shm.data->entries[queue_number].lpr.m);
        strcpy(shm.data->entries[queue_number].lpr.rego, EMPTY_LPR);
        pthread_mutex_unlock(&shm.data->entries[queue_number].lpr.m);
    }

    return ptr;
}

void *exitManager(void *ptr)
{
    int queue_number = *((int *)ptr);

    // Make sure LPR is Empty to start
    pthread_mutex_lock(&shm.data->exits[queue_number].lpr.m);
    strcpy(shm.data->exits[queue_number].lpr.rego, EMPTY_LPR);
    pthread_mutex_unlock(&shm.data->exits[queue_number].lpr.m);

    while (run)
    {

        // Wait for a plate to be in the LPR
        pthread_mutex_lock(&shm.data->exits[queue_number].lpr.m);
        while (strcmp(shm.data->exits[queue_number].lpr.rego, EMPTY_LPR) == 0)
        {
            // printf("waiting for lpr %d\n", queue_number);
            pthread_cond_wait(&shm.data->exits[queue_number].lpr.c, &shm.data->exits[queue_number].lpr.m);
        }
        char *plate = shm.data->exits[queue_number].lpr.rego;
        reset_rego(plate);
        pthread_mutex_unlock(&shm.data->exits[queue_number].lpr.m);

        // printf("car with plate %s triggered lpr at exit %d\n", plate, queue_number);

        if (shm.data->exits[queue_number].boomgate.s != 'O')
        {

            pthread_mutex_lock(&entryBoomObjm[queue_number].m);
            pthread_cond_signal(&entryBoomObjm[queue_number].c);
            pthread_mutex_unlock(&entryBoomObjm[queue_number].m);
        }

        // Make sure LPR is Empty to end

        pthread_mutex_lock(&shm.data->exits[queue_number].lpr.m);
        strcpy(shm.data->exits[queue_number].lpr.rego, EMPTY_LPR);
        pthread_mutex_unlock(&shm.data->exits[queue_number].lpr.m);
    }

    return ptr;
}

void *entryBoomManager(void *ptr)
{
    int boom_number = *((int *)ptr);

    // Make sure Boomgate is closed to start
    pthread_mutex_lock(&shm.data->entries[boom_number].boomgate.m);
    shm.data->entries[boom_number].boomgate.s = 'C';
    pthread_mutex_unlock(&shm.data->entries[boom_number].boomgate.m);

    while (run)
    {

        pthread_mutex_lock(&entryBoomObjm[boom_number].m);

        pthread_cond_wait(&entryBoomObjm[boom_number].c, &entryBoomObjm[boom_number].m);

        // printf("boom %d broke\n", boom_number);
        pthread_mutex_unlock(&entryBoomObjm[boom_number].m);

        pthread_mutex_lock(&shm.data->entries[boom_number].boomgate.m);

        if (shm.data->entries[boom_number].boomgate.s == 'L')
        {
            while (shm.data->entries[boom_number].boomgate.s == 'L')
            {
                pthread_cond_wait(&shm.data->entries[boom_number].boomgate.c, &shm.data->entries[boom_number].boomgate.m);
            }
        }

        shm.data->entries[boom_number].boomgate.s = 'R';

        pthread_cond_signal(&shm.data->entries[boom_number].boomgate.c);
        // pthread_mutex_unlock(&shm.data->entries[boom_number].boomgate.m);

        while (shm.data->entries[boom_number].boomgate.s != 'O')
        {
            pthread_cond_wait(&shm.data->entries[boom_number].boomgate.c, &shm.data->entries[boom_number].boomgate.m);
            // printf("boom %d woke\n", boom_number);
        }

        miliSleep(20);

        // printf("boom %d wait\n", boom_number);
        // pthread_mutex_lock(&shm.data->entries[boom_number].boomgate.m);
        shm.data->entries[boom_number].boomgate.s = 'L';
        pthread_cond_signal(&shm.data->entries[boom_number].boomgate.c);
        pthread_mutex_unlock(&shm.data->entries[boom_number].boomgate.m);
        // printf("boom %d signaled sim to close\n", boom_number);
    }

    return ptr;
}

void *lvlManager(void *ptr)
{
    // int lvl_number = *((int *)ptr);

    // pthread_mutex_lock(&shm.data->levels[lvl_number].lpr.m);

    // strcpy(shm.data->levels[lvl_number].lpr.rego, EMPTY_LPR);
    // pthread_mutex_unlock(&shm.data->levels[lvl_number].lpr.m);
    // char rego[6];
    // bool skip = false;

    // pthread_mutex_lock(&car_per_lvl[lvl_number].m);
    // for(int x; x<CARPARKS_PER_LEVEL;x++){
    //     strcpy(car_per_lvl[lvl_number].cars[x].carRego, EMPTY_LPR);
    // }
    // pthread_mutex_unlock(&car_per_lvl[lvl_number].m);
    // while ( run ){

    //     skip = false;
    //     pthread_mutex_lock(&shm.data->levels[lvl_number].lpr.m);

    //     while(strcmp(shm.data->levels[lvl_number].lpr.rego, EMPTY_LPR) == 0){

    //         pthread_cond_wait(&shm.data->levels[lvl_number].lpr.c, &shm.data->levels[lvl_number].lpr.m);
    //     }

    //     strcpy(rego, shm.data->levels[lvl_number].lpr.rego);
    //     pthread_mutex_unlock(&shm.data->levels[lvl_number].lpr.m);
    //     pthread_mutex_lock(&car_per_lvl[lvl_number].m);
    //     for(int i; i<CARPARKS_PER_LEVEL;i++){
    //         //printf("at check for %s with %s\n", car_per_lvl[lvl_number].cars[i].carRego, rego);

    //         if(strcmp(car_per_lvl[lvl_number].cars[i].carRego, rego) == 0){
    //             //printf("Car removed\n");
    //             strcpy(car_per_lvl[lvl_number].cars[i].carRego, EMPTY_LPR);
    //             car_per_lvl[lvl_number].actualCounter--;
    //             car_per_lvl[lvl_number].assignedCounter--;
    //             //pthread_mutex_unlock(&car_per_lvl[lvl_number].m);

    //             skip = true;
    //         }

    //     }
    //     pthread_mutex_unlock(&car_per_lvl[lvl_number].m);

    //     if(!skip){
    //         for(int i; i<CARPARKS_PER_LEVEL;i++){
    //         pthread_mutex_lock(&car_per_lvl[lvl_number].m);

    //         if(strcmp(car_per_lvl[lvl_number].cars[i].carRego, EMPTY_LPR) == 0){
    //             strcpy(car_per_lvl[lvl_number].cars[i].carRego, rego);
    //             car_per_lvl[lvl_number].actualCounter++;

    //         }

    //         pthread_mutex_unlock(&car_per_lvl[lvl_number].m);
    //     }
    //     }

    // }
    return ptr;
}

void *exitBoomManager(void *ptr)
{
    int boom_number = *((int *)ptr);

    // Make sure Boomgate is closed to start
    pthread_mutex_lock(&shm.data->exits[boom_number].boomgate.m);
    shm.data->exits[boom_number].boomgate.s = 'C';
    pthread_mutex_unlock(&shm.data->exits[boom_number].boomgate.m);

    while (run)
    {

        pthread_mutex_lock(&entryBoomObjm[boom_number].m);

        pthread_cond_wait(&entryBoomObjm[boom_number].c, &entryBoomObjm[boom_number].m);

        // //printf("boom %d broke\n", boom_number);
        pthread_mutex_unlock(&entryBoomObjm[boom_number].m);

        pthread_mutex_lock(&shm.data->exits[boom_number].boomgate.m);

        if (shm.data->exits[boom_number].boomgate.s == 'L')
        {
            while (shm.data->exits[boom_number].boomgate.s == 'L')
            {
                pthread_cond_wait(&shm.data->exits[boom_number].boomgate.c, &shm.data->exits[boom_number].boomgate.m);
            }
        }

        shm.data->exits[boom_number].boomgate.s = 'R';

        pthread_cond_signal(&shm.data->exits[boom_number].boomgate.c);
        // pthread_mutex_unlock(&shm.data->entries[boom_number].boomgate.m);

        while (shm.data->exits[boom_number].boomgate.s != 'O')
        {
            pthread_cond_wait(&shm.data->exits[boom_number].boomgate.c, &shm.data->exits[boom_number].boomgate.m);
            // printf("boom %d woke\n", boom_number);
        }
        pthread_mutex_unlock(&shm.data->exits[boom_number].boomgate.m);

        miliSleep(20);
        pthread_mutex_lock(&shm.data->exits[boom_number].boomgate.m);

        // printf("boom %d wait\n", boom_number);
        // pthread_mutex_lock(&shm.data->exits[boom_number].boomgate.m);
        shm.data->exits[boom_number].boomgate.s = 'L';
        pthread_cond_signal(&shm.data->exits[boom_number].boomgate.c);
        pthread_mutex_unlock(&shm.data->exits[boom_number].boomgate.m);
        // printf("boom %d signaled sim to close\n", boom_number);
    }

    return ptr;
}

/*
    Allocate level
*/
int allocate_level()
{
    for (int i = 0; i < LEVELS; i++)
    {
        // for(int s = 0; s<CARPARKS_PER_LEVEL; s++){
        pthread_mutex_lock(&car_per_lvl[i].m);
        if (car_per_lvl[i].assignedCounter < CARPARKS_PER_LEVEL)
        {
            // strcpy(car_per_lvl[i].cars[s], rego);
            car_per_lvl[i].assignedCounter = car_per_lvl[i].assignedCounter + 1;
            pthread_mutex_unlock(&car_per_lvl[i].m);
            return i;
        }
        pthread_mutex_unlock(&car_per_lvl[i].m);

        //}
    }
    return -1;
}

/*
    Billing thread
*/
void *billing(void *ptr)
{
    bool billrun = true;
    while (billrun)
    {
        increment_all(&rego_table);
        miliSleep(1);
    }

    return ptr;
}

/*
    Reset billing for rego
*/
void reset_rego(char *rego)
{
    rego_t *b = htab_find(&rego_table, rego);
    double cost = b->value * 0.05;      // Calculates cost
    set_rego_value(&rego_table, b, -1); // -1 is set to inactive vehicle
    if (cost > 0.00)
    {
        char toWrite[100];                            // Big enough buffer
        sprintf(toWrite, "%s $%.2f\n", b->key, cost); // Rego $Cost
        FILE *fp = fopen("./billing.txt", "a+");
        for (int i = 0; toWrite[i] != '\0'; i++)
        { // \0 EOF
            fputc(toWrite[i], fp);
        }
        totalRevenue = totalRevenue + cost;
        fclose(fp);
    }
}

/*
    50ms Process function
*/
void *process(void *ptr)
{
    while (true)
    {
        displayUI();   // Display UI
        miliSleep(50); // Sleep for 50ms every loop
    }
}

/*
    Remove char from string
*/
void removeChar(char *str, char c)
{
    int i = 0;
    int j = 0;

    while (str[i])
    {
        if (str[i] != c)
        {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = 0;
}

/*
    Reads regos from plates.txt into hash table
*/
void read_rego_to_table(void)
{
    int ch, lines = 0;

    // Count number of lines in file
    FILE *myfile = fopen("./plates.txt", "r");
    do
    {
        ch = fgetc(myfile);
        if (ch == '\n')
            lines++;
    } while (ch != EOF);

    rewind(myfile);

    char *contents[lines];
    int i = 0;
    size_t len = 0;
    for (i = 0; i < lines; i++)
    {
        contents[i] = NULL;
        len = 0;
        getline(&contents[i], &len, myfile);
        removeChar(contents[i], '\r'); // Remove new line chars
        removeChar(contents[i], '\n');
    }
    fclose(myfile); // Closes file

    // Add to hash table
    for (int i = 0; i < lines; i++)
    {
        htab_add(&rego_table, contents[i], -1);
    }
}

/*
    Get a shared memory object
*/
bool get_shared_object(shared_memory_t *shm)
{
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd. If the operation fails, ensure that shm->data is
    // NULL and return false.
    if ((shm->fd = shm_open(shm->name, O_RDWR, 0666)) < 0)
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

/*
    Display levels for testing
*/
void display_levels()
{
    // for(int i=0;i<LEVELS;i++){
    //     for(int b=0;b<car_per_lvl[i].counter;b++){
    //         //printf("Car %s on level %d total on level %d\n", car_per_lvl[i].cars[b], i, car_per_lvl[i].counter);
    //     }
    // }
}

/*
    Remove from level
*/
void remove_from_level(int lprLevel)
{

    // for(int b=0;b<car_per_lvl[lprLevel].counter;b++){

    // if(strcmp(car_per_lvl[lprLevel].cars[b], rego) == 0){
    pthread_mutex_lock(&car_per_lvl[lprLevel].m);

    // strcpy(car_per_lvl[lprLevel].cars[b], EMPTY_LPR);

    car_per_lvl[lprLevel].assignedCounter--;
    car_per_lvl[lprLevel].actualCounter--;

    pthread_mutex_unlock(&car_per_lvl[lprLevel].m);
    // break;
    //}
    //}
}

/*
    Display the GUI screen
    - Boomgates sign done
    - Temperatures done
    - Digital signs done
    Todo
    - LPR
    - Vehicles and max each level
    - total billing
*/
void displayUI(void)
{
    system("clear"); // Clear screen

    printf("Parking Garage Simulation\n\n");
    // Car Spots Per Level
    for (int i = 0; i < LEVELS; i++)
    {
        printf("Level %d: %d/%d\n", i + 1, car_per_lvl[i].actualCounter, CARPARKS_PER_LEVEL);
    }
    printf("\n");
    // Entry and Exit Boomgate
    for (int i = 0; i < ENTRANCES; i++)
    {
        printf("Entrance %d: %s         Exit %d: %s\n", i + 1, &shm.data->entries[i].boomgate.s, i + 1, &shm.data->exits[i].boomgate.s);
    }
    printf("\n");
    // Alarm Sensor and Temperature Sensor
    for (int i = 0; i < LEVELS; i++)
    {
        printf("Alarm Sensor %d: %d         Temp Sensor %d: %d\n", i + 1, shm.data->levels[i].alarm, i + 1, shm.data->levels[i].tempnode.temperature);
    }
    printf("\n");
    // Entrance Sign and LPR
    for (int i = 0; i < ENTRANCES; i++)
    {
        printf("Entrance %d Sign: [  %s   ]         LPR %d: %s\n", i + 1, &shm.data->entries[i].parkingsign.display, i + 1, shm.data->entries[i].lpr.rego);
    }
    printf("\n");
    printf("Revenue collected: %.2f\n", totalRevenue);
}