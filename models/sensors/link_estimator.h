#ifndef SENSORSNETWORKMODELPROJECT_LINK_ESTIMATOR_H
#define SENSORSNETWORKMODELPROJECT_LINK_ESTIMATOR_H

#include <stdbool.h>

typedef struct _ctp_routing_packet ctp_routing_packet;
typedef struct _ctp_link_estimator_frame ctp_link_estimator_frame;
typedef struct _node_state node_state;
typedef double simtime_t;

/*
 * Structure that describes an entry in the link estimator table (or neighbor table): it reports the features of a link
 * to a neighbor node
 */

typedef struct _link_estimator_table_entry{
        unsigned int neighbor; // ID of the neighbor
        unsigned char lastseq; // Last beacon sequence number received from the neighbor

        /*
         * Number of beacons received after the last update of the outgoing link quality: such an update takes place
         * after BLQ_PKT_WINDOW beacons have been received
         */

        unsigned char beacons_received;

        /*
         * Number of beacons missed after the last update of the outgoing link quality: such an update takes place
         * after BLQ_PKT_WINDOW beacons have been received
         */

        unsigned char beacons_missed;
        unsigned char flags; // Flags describing the state of the entry (see above)
        unsigned char ingoing_quality; // Ingoing quality of the link ranges from 1 (bad) to 255 (good)
        unsigned short one_hop_etx; // 1-hop etx of the neighbor

        /*
         * Number of data packets acknowleged after the last update of the outgoing link quality: such an update takes
         * place after DLQ_PKT_WINDOW data packets have been sent
         */

        unsigned char data_acknowledged;

        /*
         * Number of data packets transmitted after the last update of the outgoing link quality: such an update takes
         * place after DLQ_PKT_WINDOW data packets have been sent
         */

        unsigned char data_sent;
}link_estimator_table_entry;

/*
 * Flags for the neighbor table entry
 */

enum {

        /*
         * The entry corresponding to a neighbor becomes invalid if no beacon is received from him within a certain
         * timeout
         */

                VALID_ENTRY = 0x1,

        /*
         * A link becomes mature after BLQ_PKT_WINDOW packets are received and an estimate is computed
         */

                MATURE_ENTRY = 0x2,

        /*
         * Flag to indicate that this link has received the first sequence number
         */

                INIT_ENTRY = 0x4,

        /*
         * Flag indicates that the 1-hop ETX of the neighbor is 0, thus it's the root of the tree; also it indicates
         * that the node is selected as the current parent node
         */

                PINNED_ENTRY = 0x8
};

/* LINK ESTIMATOR API */

unsigned short get_one_hop_etx(unsigned int address,link_estimator_table_entry* link_estimator_table);
bool unpin_neighbor(unsigned int address,link_estimator_table_entry* link_estimator_table);
bool clear_data_link_quality(unsigned int address,link_estimator_table_entry* link_estimator_table);
bool send_routing_packet(node_state* state);
void receive_routing_packet(void* message,node_state* state);
bool pin_neighbor(unsigned int address,link_estimator_table_entry* link_estimator_table);
int insert_neighbor(unsigned int neighbor,link_estimator_table_entry* link_estimator_table);
void ack_received(unsigned int recipient,bool ack_received,link_estimator_table_entry* link_estimator_table);
void init_link_estimator_table(link_estimator_table_entry* link_estimator_table);
bool compare_link_estimator_frames(ctp_link_estimator_frame* a,ctp_link_estimator_frame* b);

#endif
