#define actual_malloc(siz) malloc(siz)
#define actual_free(ptr) free(ptr)
#define actual_realloc(ptr, size) realloc(ptr, size)

#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>

#include <datatypes/hash_map.h>
#include <datatypes/array.h>

#include "common.h"

enum _actions{
	ADD,
	REMOVE,
	CHECK,
	ACTIONS_NUM
};

struct test_e{
	unsigned long long key;
	unsigned long long value;
};

void * __wrap_malloc(size_t size){
	return actual_malloc(size);
}

void __wrap_free(void *ptr){
	return actual_free(ptr);
}

void * __wrap_realloc(void *ptr, size_t size){
	return actual_realloc(ptr, size);
}

static rootsim_array(struct test_e *) testset;
static struct rootsim_hash_map_t hashmap;
static unsigned long curr_value = 0;

static void do_add(void)
{
	bool found;
	unsigned i;
	unsigned long long key;
	struct test_e *aux;

	do{
		found = false;
		i = array_count(testset);
		key = rand() + curr_value;
		while(i--){
			if(array_get_at(testset, i)->key == key){
				found = true;
				break;
			}
		}

	} while(found);

	aux = actual_malloc(sizeof(*aux));
	if(!aux){
		printf("[ADD] out of memory!!!");
		abort();
		return;
	}

	aux->key = key;
	aux->value = curr_value;
	curr_value++;

	array_push(testset, aux);
	hash_map_add(&hashmap, (key_elem_t *) aux);
}

static void do_remove()
{
	unsigned i;
	unsigned long long key, value;
	struct test_e *aux;

	if(!array_count(testset))
		return;

	i = rand() % array_count(testset);
	value = array_get_at(testset, i)->value;
	key = array_get_at(testset, i)->key;
	aux = (struct test_e *)hash_map_lookup(&hashmap, key);
	if(!aux){
		printf("[REMOVE] element not found!!!");
		abort();
		return;
	}
	if(key != aux->key){
		printf("[REMOVE] bad key!!!");
		abort();
		return;
	}
	if(value != aux->value){
		printf("[REMOVE] bad value!!!");
		abort();
		return;
	}

	hash_map_remove(&hashmap, key);
	__wrap_free(array_lazy_remove_at(testset, i));
}

static void do_check()
{
	unsigned i;
	unsigned long long key, value;
	struct test_e *aux;

	if(array_count(testset) != hash_map_count(&hashmap)){
		printf("[CHECK] bad size found!!!");
		abort();
		return;
	}

	i = array_count(testset);
	while(i--){
		value = array_get_at(testset, i)->value;
		key = array_get_at(testset, i)->key;
		aux = (struct test_e *)hash_map_lookup(&hashmap, key);
		if(!aux){
			printf("[CHECK] element not found!!!");
			abort();
			return;
		}
		if(key != aux->key){
			printf("[CHECK] bad key!!!");
			abort();
			return;
		}
		if(value != aux->value){
			printf("[CHECK] bad value!!!");
			abort();
			return;
		}
	}
}

static void do_check_iter()
{
	unsigned i;
	unsigned char *dump, *ptr;
	struct test_e *aux;

	dump = actual_malloc(array_dump_size(testset));
	if(!dump){
		printf("[CHECK ITER] out of memory!!!");
		abort();
		return;
	}
	ptr = dump;
	array_dump(testset, ptr);

	map_size_t iter = 0;

	while((aux = (struct test_e *) hash_map_iter(&hashmap, &iter))){
		i = array_count(testset);
		while(i--){
			if(array_get_at(testset, i)->key == aux->key){
				array_lazy_remove_at(testset, i);
				break;
			}
		}
	}

	if(!array_empty(testset)){
		printf("[CHECK ITER] bad iter!!!");
		abort();
		return;
	}

	array_fini(testset);
	ptr = dump;
	array_load(testset, ptr);
	actual_free(dump);
}

int main(){
	hash_map_init(&hashmap);
	array_init(testset);

	srand(time(NULL));

	unsigned j = 100000;
	bool found;
	while(j--){
		switch(rand() % ACTIONS_NUM){
			case ADD:
				do_add();
				break;

			case REMOVE:
				do_remove();
				break;

			case CHECK:
				do_check();
				if(!(rand() % 10))
					do_check_iter();
				break;
		}
	}

	while(!array_empty(testset)){
		__wrap_free(array_pop(testset));
	}

	array_fini(testset);
	hash_map_fini(&hashmap);
	return 0;
}
