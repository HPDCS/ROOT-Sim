#pragma once

#include <stdbool.h>

typedef struct {
	char *key;
	void *value;
} kv_pair_t;

typedef struct __dict {
	kv_pair_t *head;
	struct __dict *tail;
} dict_t;

dict_t *dict_new();
void dict_add(dict_t *dict, const char *key, void *value);
int dict_has(dict_t *dict, const char *key);
void *dict_get(dict_t *dict, const char *key);
void dict_remove(dict_t *dict, const char *key);
void dict_free(dict_t *dict, bool free_value);
void dict_iter(dict_t *dict, void (*func)(void *));
