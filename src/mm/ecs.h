#pragma once


typedef struct _ecs_page_node {
	
} ecs_page_node_t;

typedef struct _ecs_page_request {
	void *base_address;
	bool write_mode;
	unsigned int count;
	unsigned char buffer[];
} ecs_page_request_t;

extern void lp_alloc_deschedule(void);
extern void lp_alloc_schedule(void);
extern void lp_alloc_thread_init(void);
extern void setup_ecs_on_segment(msg_t *);
extern void ecs_send_pages(msg_t *);
extern void ecs_install_pages(msg_t *);
void unblock_synchronized_objects(LID_t lid);

#ifdef HAVE_ECS
extern void remote_memory_init(void);
#else
#define remote_memory_init()
#endif
