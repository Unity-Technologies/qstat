/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Haze query protocol
 * Copyright 2007 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_HAZE_H
#define QSTAT_HAZE_H

#include "qserver.h"

// Packet processing methods
void deal_with_haze_packet( struct qserver *server, char *pkt, int pktlen );
void deal_with_haze_status( struct qserver *server, char *rawpkt, int pktlen );
void send_haze_request_packet( struct qserver *server );

#endif
