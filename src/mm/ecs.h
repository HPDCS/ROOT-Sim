#pragma once

#ifndef _ECS_H
#define _ECS_H

#include <datatypes/bitmap.h>
#include <mm/mm.h>

#define NO_PREFETCH 0
#define CLUSTERED 1
#define SCATTERED 2

#define max_pages 1048576
#define alpha 1

typedef struct _ecs_page_node_t {
	struct	_ecs_page_node_t *next;
	struct	_ecs_page_node_t *prev;
	long long  page_address;
	size_t pages;
	rootsim_bitmap write_mode[];
} ecs_page_node_t;

typedef struct _ecs_page_request {
	void *base_address;
	bool write_mode;
	int prefetch_mode;
	unsigned int count;
	unsigned char buffer[];
} ecs_page_request_t;

#define INITIAL_WRITEBACK_SLOTS	2

typedef struct prefetch_page{
	long long address;
	int write_mode;
	unsigned char page[PAGE_SIZE];
} prefetch_page_t;

typedef struct _ecs_prefetch{
	size_t count;
	size_t total;
	prefetch_page_t pages[];
} ecs_prefetch_t;

typedef struct writeback_page {
	long long address;
	unsigned char page[PAGE_SIZE];
} writeback_page_t;

typedef struct _ecs_writeback {
	size_t count;
	size_t total;
	writeback_page_t pages[];
} ecs_writeback_t;

extern void lp_alloc_deschedule(void);
extern void lp_alloc_schedule(void);
extern void lp_alloc_thread_init(void);
extern void setup_ecs_on_segment(msg_t *);
extern void ecs_send_pages(msg_t *);
extern void reinstall_writeback_pages(msg_t *msg);
extern void ecs_install_pages(msg_t *);
void unblock_synchronized_objects(LID_t lid);

ecs_prefetch_t *add_prefetch_page(ecs_prefetch_t *pfr, long long *address, int span, int write_mode);
ecs_prefetch_t * prefetch_init(void);
ecs_page_node_t *add_page_node(long long address, size_t pages, LID_t lid);
#endif /* _ECS_H */

