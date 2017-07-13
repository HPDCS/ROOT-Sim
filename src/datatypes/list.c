/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file list.c
* @brief This module implemets the general-purpose list implementation used
*	 in the simulator
* @author Alessandro Pellegrini
* @date November 5, 2013
*/



#include <datatypes/list.h>
#include <mm/dymelor.h>


/**
* This function inserts a new element at the beginning of the list.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_insert_head() macro (defined in <datatypes/list.h>) which sets
* correctly many parameters, and provides a more useful API.
* This function allocates new memory to copy the payload pointed by data. If
* data points to a malloc'd data structure, then the caller should free it after
* calling list_insert(), to prevent memory leaks.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_insert_head() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert_head() macro.
* @param data a pointer to the data buffer to be inserted into the list.
*
* @return a pointer to the newly-allocated copy of the payload into the list.
*           This is different from the container node, which is not exposed to the caller.
*/
char *__list_insert_head(unsigned int lid, void *li, unsigned int size, void *data) {

	rootsim_list *l = (rootsim_list *)li;

	assert(l);

	size_t size_before = l->size;

	struct rootsim_list_node *new_n;

	// Create the new node and populate the entry
	new_n = umalloc(lid, sizeof(struct rootsim_list_node) + size);
	bzero(new_n, sizeof(struct rootsim_list_node) + size);
	memcpy(&new_n->data, data, size);

	// Is the list empty?
	if(l->size == 0) {
		l->head = new_n;
		l->tail = new_n;
		goto insert_end;
	}

	// Otherwise add at the beginning
	new_n->prev = NULL;
	new_n->next = l->head;
	l->head->prev = new_n;
	l->head = new_n;

    insert_end:
	l->size++;
	assert(l->size == (size_before + 1));

	return new_n->data;
}




/**
* This function inserts a new element at the end of the list.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_insert_head() macro (defined in <datatypes/list.h>) which sets
* correctly many parameters, and provides a more useful API.
* This function allocates new memory to copy the payload pointed by data. If
* data points to a malloc'd data structure, then the caller should free it after
* calling list_insert(), to prevent memory leaks.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_insert_tail() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
* @param data a pointer to the data buffer to be inserted into the list.
*
* @return a pointer to the newly-allocated copy of the payload into the list.
*           This is different from the container node, which is not exposed to the caller.
*/
char *__list_insert_tail(unsigned int lid, void *li, unsigned int size, void *data) {

	rootsim_list *l = (rootsim_list *)li;

	assert(l);
	size_t size_before = l->size;

	struct rootsim_list_node *new_n;

	// Create the new node and populate the entry
	new_n = umalloc(lid, sizeof(struct rootsim_list_node) + size);
	bzero(new_n, sizeof(struct rootsim_list_node) + size);
	memcpy(&new_n->data, data, size);

	// Is the list empty?
	if(l->size == 0) {
		l->head = new_n;
		l->tail = new_n;
		goto insert_end;
	}

	// Otherwise add at the end
	new_n->next = NULL;
	new_n->prev = l->tail;
	l->tail->next = new_n;
	l->tail = new_n;

    insert_end:
	l->size++;
	assert(l->size == (size_before + 1));
	return new_n->data;
}


void dump_l(struct rootsim_list_node *n, size_t key_position) {
	while(n != NULL) {
		printf("%f -> ", get_key(&n->data));
		n = n->next;
	}
	printf("\n");
}


char *__list_insert(unsigned int lid, void *li, unsigned int size, size_t key_position, void *data) {
	struct rootsim_list_node *new_n;
	
	new_n = list_allocate_node(lid, size);
	bzero(new_n, sizeof(struct rootsim_list_node) + size);
	memcpy(&new_n->data, data, size);

	return __list_place(lid, li, key_position, new_n);
}

/**
* This function inserts a new element into the specified ordered doubly-linked list.
* The paylaod of a new node can be any pointer to any data structure, provided that
* the key to be used for ordering is either a long long or a double.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_insert() macro (defined in <datatypes/list.h>) which sets
* correctly many parameters, and provides a more useful API.
* This function allocates new memory to copy the payload pointed by data. If
* data points to a malloc'd data structure, then the caller should free it after
* calling list_insert(), to prevent memory leaks.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_insert() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
* @param key_position offset (in bytes) of the key field in the data structure kept
*           by the list. This is used to maintain the list ordered.
* @param data a pointer to the data buffer to be inserted into the list.
*
* @return a pointer to the payload of the node just linked to the list.
*           This is different from the container node, which is not exposed to the caller.
*/
char *__list_place(unsigned int lid, void *li, size_t key_position, struct rootsim_list_node *new_n) {

	rootsim_list *l = (rootsim_list *)li;

	assert(l);
	size_t size_before = l->size;

	struct rootsim_list_node *n;

	double key = get_key(new_n->data);

	// Is the list empty?
	if(l->size == 0) {
		new_n->prev = NULL;
		new_n->next = NULL;
		l->head = new_n;
		l->tail = new_n;
		goto insert_end;
	}

	n = l->tail;
	while(n != NULL && key < get_key(&n->data)) {
		n = n->prev;
	}

	// Insert correctly depending on the position
 	if(n == l->tail) { // tail
		new_n->next = NULL;
		l->tail->next = new_n;
		new_n->prev = l->tail;
		l->tail = new_n;
	} else if(n == NULL) { // head
		new_n->prev = NULL;
		new_n->next = l->head;
		l->head->prev = new_n;
		l->head = new_n;
	} else { // middle
		new_n->prev = n;
		new_n->next = n->next;
		n->next->prev = new_n;
		n->next = new_n;
	}


    insert_end:
	l->size++;
	assert(l->size == (size_before + 1));
	return new_n->data;
}




/**
* This function extracts an element from the list, if a corresponding key value is
* found in any element.
* The payload of the list node is copied into a newly malloc'd buffer, which
* is returned by this function (if a node is found). After calling this function,
* the node containing the original copy of the payload is free'd.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_extract() macro (defined in <datatypes/list.h>) which sets
* correctly many parameters, and provides a more useful API.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_extract() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
* @param key the key value to perform list search.
* @param key_position offset (in bytes) of the key field in the data structure kept
*           by the list. This is used to maintain the list ordered.
*
* @return a pointer to the payload of the corresponding node, if found, or NULL.
*/
char *__list_extract(unsigned int lid, void *li, unsigned int size, double key, size_t key_position) {

	rootsim_list *l = (rootsim_list *)li;

	assert(l);
	size_t size_before = l->size;

	char *content = NULL;
	struct rootsim_list_node *n = l->head;
	double curr_key;

	while(n != NULL) {
		curr_key = get_key(&n->data);
		if(D_EQUAL(curr_key, key)) {
			if(l->head == n) {
				l->head = n->next;
				l->head->prev = NULL;
			}

			if(l->tail == n) {
				l->tail = n->prev;
				l->tail->next = NULL;
			}

			if(n->next != NULL) {
				n->next->prev = n->prev;
			}

			if(n->prev != NULL) {
				n->prev->next = n->next;
			}

			content = umalloc(lid, size);
			memcpy(content, &n->data, size);
			n->next = (void *)0xDEADBEEF;
			n->prev = (void *)0xDEADBEEF;
			bzero(n->data, size);
			ufree(lid, n);

			l->size--;
			assert(l->size == (size_before - 1));
			return content;
		}
		n = n->next;
	}

	return content;
}


/**
* This function deletes an element from the list, if a corresponding key value is
* found in any element.
* After calling this function, the node containing the original copy of the payload is free'd.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_delete() macro (defined in <datatypes/list.h>) which sets
* correctly many parameters, and provides a more useful API.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_delete() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
* @param key the key value to perform list search.
* @param key_position offset (in bytes) of the key field in the data structure kept
*           by the list. This is used to maintain the list ordered.
*
* @return true, if the node was found and deleted. false, otherwise
*/
bool __list_delete(unsigned int lid, void *li, unsigned int size, double key, size_t key_position) {
	void *content;
	if((content =__list_extract(lid, li, size, key, key_position)) != NULL) {
		bzero(&content, size);
		ufree(lid, content);
		return true;
	}
	return false;
}



/**
* This function extracts an element from the list, starting from the memory address of
* the payload of the list node.
* After calling this function, the node containing the original copy of the payload is free'd.
* If copy is set, then the original element to be extracted is copied into a newly-allocated
* buffer and returned. This is useful to implement both extract_by_content and delete_by_content.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there are the list_extract_by_content() and list_delete_by_content() macros
* (defined in <datatypes/list.h>) which set correctly many parameters, and provide a more useful API.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_extract_by_content() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
* @param ptr a pointer to the payload of a particular list node to be extracted/deleted.
* @param copy if this flag is set, a copy of the payload is made before free'ing the list node.
*
* @return a pointer to the payload's copy if copy is set, NULL otherwise
*/
char *__list_extract_by_content(unsigned int lid, void *li, unsigned int size, void *ptr, bool copy) {

        rootsim_list *l = (rootsim_list *)li;

	assert(l);
	size_t size_before = l->size;

	struct rootsim_list_node *n = list_container_of(ptr);
	char *content = NULL;

	if(l->head == n) {
		l->head = n->next;
		if(l->head != NULL) {
			l->head->prev = NULL;
		}
	}

	if(l->tail == n) {
		l->tail = n->prev;
		if(l->tail != NULL) {
			l->tail->next = NULL;
		}
	}

	if(n->next != NULL) {
		n->next->prev = n->prev;
	}

	if(n->prev != NULL) {
		n->prev->next = n->next;
	}

	if(copy) {
		content = umalloc(lid, size);
		memcpy(content, &n->data, size);
	}
	n->next = (void *)0xDEADC0DE;
	n->prev = (void *)0xDEADC0DE;
//	bzero(n->data, size);
	ufree(lid, n);

	l->size--;
	assert(l->size == (size_before - 1));

	return content;
}




/**
* This function finds an element in the list given its key.
* The pointer returned by this function points to the actual payload of the node, not
* a copy. Modifying the element pointed affects any other thread scanning the list.
* If the node is deleted/extracted by any other thread operating on the list, then
* accessing the pointer returned might produce undefined behaviour.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_find() macro (defined in <datatypes/list.h>) which sets correctly
* many parameters, and provide a more useful API.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_find() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param key the key value to perform list search.
* @param key_position offset (in bytes) of the key field in the data structure kept
*           by the list. This is used to maintain the list ordered.
*
* @return a pointer to the payload of the list node mathing the key, NULL if no
*         matching node is found.
*/
char *__list_find(void *li, double key, size_t key_position) {

        rootsim_list *l = (rootsim_list *)li;

	assert(l);

	struct rootsim_list_node *n = l->head;
	double curr_key;

	while(n != NULL) {
		// First of all, get the key for current node's structure...
		curr_key = get_key(&n->data);
		// ...and then compare it!
		if(D_EQUAL(curr_key, key)) {
			return n->data;
		}
		n = n->next;
	}

	return NULL;
}



/**
* This function deletezs the first element of the list, if the list is not empty.
* It is not safe (and not easily readable) to call this function directly. Rather,
* there is the list_pop() macro (defined in <datatypes/list.h>) which sets correctly
* many parameters, and provide a more useful API.
*
* @author Alessandro Pellegrini
*
* @param li a pointer to the list data strucuture. Note that if passed through the
*           list_pop() macro, the type of the pointer is the same as the content,
*           but the pointed memory contains a buffer whose actual type is rootsim_list.
*           This allows to retrieve the size of the data being kept by this current
*           incarnation of the list, but it is not safe to access the list directly.
* @param size the size of the payload of the list. This is automatically set by
*           the list_insert() macro.
*/
void list_pop(unsigned int lid, void *li) {

        rootsim_list *l = (rootsim_list *)li;

	assert(l);
	size_t size_before = l->size;

	struct rootsim_list_node *n = l->head;
	struct rootsim_list_node *n_next;

	if(n != NULL) {
		l->head = n->next;
		if(n->next != NULL) {
			n->next->prev = NULL;
		}
		n_next = n->next;
		n->next = (void *)0xDEFEC8ED;
		n->prev = (void *)0xDEFEC8ED;
		ufree(lid, n);
		n = n_next;
		l->size--;
		assert(l->size == (size_before - 1));
	}
}


// element associated with key is not truncated
unsigned int __list_trunc(unsigned int lid, void *li, double key, size_t key_position, unsigned short int direction) {

	struct rootsim_list_node *n;
	struct rootsim_list_node *n_adjacent;
	rootsim_list *l = (rootsim_list *)li;
	unsigned int deleted = 0;

	assert(l);
	size_t size_before = l->size;

	// Attempting to truncate an empty list?
	if(l->size == 0) {
		goto out;
	}

	if(direction == LIST_TRUNC_AFTER) {
		printf("not implemented\n");
		abort();
	}


	n = l->head;
	while(n != NULL && get_key(&n->data) < key) {
		deleted++;
                n_adjacent = n->next;
                n->next = (void *)0xBAADF00D;
                n->prev = (void *)0xBAADF00D;
                ufree(lid, n);
                n = n_adjacent;
	}
	l->head = n;
	if(l->head != NULL)
		l->head->prev = NULL;


	l->size -= deleted;
	assert(l->size == (size_before - deleted));
    out:
	return deleted;
}


void *list_allocate_node(unsigned int lid, size_t size) {
	struct rootsim_list_node *new_n;

	new_n = umalloc(lid, sizeof(struct rootsim_list_node) + size);
	bzero(new_n, sizeof(struct rootsim_list_node) + size);
	return new_n;
}

void *list_allocate_node_buffer(unsigned int lid, size_t size) {
	char *ptr;
	
	ptr = list_allocate_node(lid, size);

	if(ptr == NULL)
		return NULL;
		
	return (void *)(ptr + sizeof(struct rootsim_list_node));
}


void list_deallocate_node_buffer(unsigned int lid, void *ptr) {
	ufree(lid, list_container_of(ptr));
}
