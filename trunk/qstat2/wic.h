/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * World in Conflict Protocol
 * Copyright 2007 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_WIC_H
#define QSTAT_WIC_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_wic_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_wic_request_packet( struct qserver *server );

#endif

