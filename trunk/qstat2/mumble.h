/*
 * qstat 2.11
 * by Steve Jankowski
 *
 * Mumble protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_MUMBLE_H
#define QSTAT_MUMBLE_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_mumble_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_mumble_request_packet( struct qserver *server );

#endif

