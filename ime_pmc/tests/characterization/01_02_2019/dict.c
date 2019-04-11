#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dict.h"

dict_t *dict_new()
{
	dict_t *dict = malloc(sizeof(dict_t));
	assert(dict != NULL);
	dict->head = NULL;
	dict->tail = NULL;
	return dict;
}

void dict_add(dict_t *dict, const char *key, void *value)
{
	if (dict_has(dict, key))
		dict_remove(dict, key);
	if (dict->head != NULL) {
		while (dict->tail != NULL) {
			dict = dict->tail;
		}
		dict_t *next = dict_new();
		dict->tail = next;
		dict = dict->tail;
	}
	int key_length = strlen(key) + 1;
	dict->head = malloc(sizeof(kv_pair_t));
	assert(dict->head != NULL);
	dict->head->key = malloc(key_length * sizeof(char));
	assert(dict->head->key != NULL);
	strcpy(dict->head->key, key);
	dict->head->value = value;
}

int dict_has(dict_t *dict, const char *key)
{
	if (dict->head == NULL)
		return 0;
	while (dict != NULL) {
		if (strcmp(dict->head->key, key) == 0)
			return 1;
		dict = dict->tail;
	}
	return 0;
}

void *dict_get(dict_t *dict, const char *key)
{
	if (dict->head == NULL)
		return NULL;
	while (dict != NULL) {
		if (strcmp(dict->head->key, key) == 0)
			return dict->head->value;
		dict = dict->tail;
	}
	return NULL;
}

void dict_remove(dict_t *dict, const char *key)
{
	if (dict->head == NULL)
		return;
	dict_t *previous = NULL;
	while (dict != NULL) {
		if (strcmp(dict->head->key, key) == 0) {
			if (previous == NULL) {
				free(dict->head->key);
				dict->head->key = NULL;
				if (dict->tail != NULL) {
					dict_t *toremove = dict->tail;
					dict->head->key = toremove->head->key;
					dict->tail = toremove->tail;
					free(toremove->head);
					free(toremove);
					return;
				}
			} else {
				previous->tail = dict->tail;
			}
			free(dict->head->key);
			free(dict->head);
			free(dict);
			return;
		}
		previous = dict;
		dict = dict->tail;
	}
}

void dict_iter(dict_t *dict, void (*func)(void *))
{
	if (dict->head == NULL)
		return;
	while (dict != NULL) {
		func(dict->head->value);
		dict = dict->tail;
	}
}

void dict_free(dict_t *dict, bool free_value)
{
	if (dict == NULL)
		return;
	free(dict->head->key);
	if(free_value)
		free(dict->head->value);
	free(dict->head);
	dict_t *tail = dict->tail;
	free(dict);
	dict_free(tail, free_value);
}
