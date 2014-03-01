/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * DirtyBomb protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_DIRTYBOMB_H
#define QSTAT_DIRTYBOMB_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_dirtybomb_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_dirtybomb_request_packet( struct qserver *server );

#endif

