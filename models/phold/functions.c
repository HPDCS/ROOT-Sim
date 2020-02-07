#include <string.h>
#include <stdio.h>

#include "application.h"

buffer* get_buffer(buffer *head, unsigned i) {
	while(i--) {
		head = head->next;
	}
	return head;
}

unsigned read_buffer(buffer *head, unsigned i)
{
	head = get_buffer(head, i);
	if(head == NULL)
		return 0;

	unsigned ret = 0;
	for (i = 0; i < head->count; i++) {
		ret ^= head->data[i];
	}

	return ret;
}

buffer* allocate_buffer(buffer *head, unsigned *data, unsigned count)
{

	buffer *new = malloc(sizeof(buffer) + count * sizeof(unsigned));
	new->next = head;
	new->count = count;

	if (data != NULL) {
		memcpy(new->data, data, count * sizeof(unsigned));
	} else {
		for(unsigned i = 0; i < count; i++) {
			new->data[i] = RandomRange(0, INT_MAX);
		}
	}

	return new;
}

buffer* deallocate_buffer(buffer *head, unsigned i)
{
	buffer *prev = NULL;
	buffer *to_free = head;

	for (unsigned j = 0; j < i; j++) {
		prev = to_free;
		to_free = to_free->next;
	}

	if (prev != NULL) {
		prev->next = to_free->next;
		free(to_free);
		return head;
	}

	prev = head->next;
	free(head);
	return prev;
}
