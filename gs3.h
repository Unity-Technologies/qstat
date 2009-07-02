/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Gamespy v3 query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GS3_H
#define QSTAT_GS3_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_gs3_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t deal_with_gs3_status( struct qserver *server, char *rawpkt, int pktlen );
query_status_t send_gs3_request_packet( struct qserver *server );

#endif
