#ifndef _DISTRIBUTED_GVT_H_
#define _DISTRIBUTED_GVT_H_

#ifdef HAS_MPI

#include <core/core.h>

#define white_0 0x0 // 0b00
#define red_0   0x1 // 0b01
#define white_1 0x2 // 0b10
#define red_1   0x3 // 0b11

#define is_red_colour(c) ( (bool) (c & 0x1) )
#define in_red_phase() ( is_red_colour(threads_phase_colour[local_tid]) )
#define next_colour(c) ( ((c)+1) & 0x3 )

extern phase_colour* threads_phase_colour;

extern simtime_t* min_outgoing_red_msg;

void gvt_comm_init(void);
void gvt_comm_finalize(void);
void enter_red_phase(void);
void exit_red_phase(void);
void join_white_msg_redux(void);
bool white_msg_redux_completed(void);
void wait_white_msg_redux(void);
bool all_white_msg_received(void);
void flush_white_msg_recv(void);
void flush_white_msg_sent(void);
void broadcast_gvt_init(unsigned int round);
bool gvt_init_pending(void);
void gvt_init_clear(void);
void join_gvt_redux(simtime_t local_vt);
bool gvt_redux_completed(void);
void join_white_msg_end(void);
bool white_msg_end_completed(void);
simtime_t last_reduced_gvt(void);
void register_incoming_msg(const msg_t*);
void register_outgoing_msg(const msg_t*);

#endif /* HAS_MPI */
#endif /* _COMM_GVT_H_ */
