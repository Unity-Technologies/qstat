/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Gamespy v2 query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GS2_H
#define QSTAT_GS2_H

#include "qserver.h"

// Packet processing methods
void deal_with_gs2_packet( struct qserver *server, char *pkt, int pktlen );
void send_gs2_request_packet( struct qserver *server );

#endif

