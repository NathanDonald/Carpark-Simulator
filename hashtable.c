#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "hashtable.h"

/*
    Hash table functions
*/
// Initialise the hashtable with N regos
bool htab_init(htab_t *h, size_t n)
{
    h->size = n;
    h->regos = (rego_t **)calloc(n, sizeof(rego_t *));
    return h->regos != 0;
}

// The Bernstein hash function.
// A very fast hash function that works well in practice.
// Reference: CAB403 Hashtable_solution.c
size_t djb_hash(char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0')
    {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Calculate the offset for the rego for key in hash table.
size_t htab_index(htab_t *h, char *key)
{
    return djb_hash(key) % h->size;
}

// Find pointer to head of list for key in hash table.
rego_t *htab_rego(htab_t *h, char *key)
{
    return h->regos[htab_index(h, key)];
}

// Find a rego for key in hash table.
rego_t *htab_find(htab_t *h, char *key)
{
    for (rego_t *i = htab_rego(h, key); i != NULL; i = i->next)
    {
        if (strcmp(i->key, key) == 0)
        { // found the key
            return i;
        }
    }
    return NULL;
}

// Add a key with value to the hash table.
int htab_add(htab_t *h, char *key, int value)
{
    // allocate new item
    rego_t *newhead = (rego_t *)malloc(sizeof(rego_t));
    if (newhead == NULL)
    {
        return false;
    }
    newhead->key = key;
    newhead->value = value;

    // hash key and place item in appropriate position
    size_t rego = htab_index(h, key);
    newhead->next = h->regos[rego];
    h->regos[rego] = newhead;
    return true;
}

// Delete an item with key from the hash table.
void htab_delete(htab_t *h, char *key)
{
    rego_t *head = htab_rego(h, key);
    rego_t *current = head;
    rego_t *previous = NULL;
    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            if (previous == NULL)
            { // first item in list
                h->regos[htab_index(h, key)] = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            break;
        }
        previous = current;
        current = current->next;
    }
}

// Destroy an initialised hash table.
void htab_destroy(htab_t *h)
{
    // free linked lists
    for (size_t i = 0; i < h->size; ++i)
    {
        rego_t *bucket = h->regos[i];
        while (bucket != NULL)
        {
            rego_t *next = bucket->next;
            free(bucket);
            bucket = next;
        }
    }

    // free regos array
    free(h->regos);
    h->regos = NULL;
    h->size = 0;
}

// Print the hash table.
void htab_print(htab_t *h)
{
    printf("hash table with %ld rego\n", h->size);
    for (size_t i = 0; i < h->size; ++i)
    {
        printf("Rego %ld: ", i);
        if (h->regos[i] == NULL)
        {
            printf("empty\n");
        }
        else
        {
            for (rego_t *j = h->regos[i]; j != NULL; j = j->next)
            {
                item_print(j);
                if (j->next != NULL)
                {
                    printf(" -> ");
                }
            }
            printf("\n");
        }
    }
}

// Print rego
void item_print(rego_t *i)
{
    printf("key=%s value=%d", i->key, i->value);
}