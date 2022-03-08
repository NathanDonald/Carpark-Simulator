#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
    Hash table methods
*/
bool htab_init(htab_t *h, size_t n);
size_t djb_hash(char *s);
size_t htab_index(htab_t *h, char *key);
rego_t *htab_rego(htab_t *h, char *key);
rego_t *htab_find(htab_t *h, char *key);
int htab_add(htab_t *h, char *key, int value);
void htab_delete(htab_t *h, char *key);
void htab_destroy(htab_t *h);
void htab_print(htab_t *h);
void item_print(rego_t *i);