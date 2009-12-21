/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Teamspeak 3 protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TS3_H
#define QSTAT_TS3_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_ts3_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_ts3_request_packet( struct qserver *server );

#endif

