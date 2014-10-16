/*
 * qstat
 * by Steve Jankowski
 *
 * Crysis protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_FARMSIM_H
#define QSTAT_FARMSIM_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_farmsim_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_farmsim_request_packet( struct qserver *server );

#endif

