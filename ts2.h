/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * Teamspeak 2 protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TS2_H
#define QSTAT_TS2_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_ts2_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_ts2_request_packet( struct qserver *server );

#endif

