/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Packet module
 * Copyright 2005 Steven Hartland based on code by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#ifndef QSTAT_PACKETS_H
#define QSTAT_PACKETS_H

#include "qstat.h"

int combine_packets( struct qserver *server );
int add_packet( struct qserver *server, unsigned int pkt_id, int pkt_index, int pkt_max, int datalen, char *data, int calc_max );
int next_sequence();
SavedData* get_packet_fragment( int index );
unsigned combined_length( struct qserver *server, int pkt_id );

#endif
