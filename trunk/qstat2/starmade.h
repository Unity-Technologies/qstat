/*
 * qstat
 * by Steve Jankowski
 *
 * StarMade protocol
 * Copyright 2013 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_STARMADE_H
#define QSTAT_STARMADE_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_starmade_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_starmade_request_packet( struct qserver *server );

#endif

