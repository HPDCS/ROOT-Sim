#pragma once

#ifndef _ECS_H
#define _ECS_H

typedef struct _ecs_page {
	
} ecs_page;

extern void lp_alloc_deschedule(void);
extern void lp_alloc_schedule(void);
extern void lp_alloc_thread_init(void);
extern void setup_ecs_on_segment(msg_t *);

#endif /* _ECS_H */

