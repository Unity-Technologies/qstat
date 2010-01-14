/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Battlefield Bad Company 2 protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_BFBC2_H
#define QSTAT_BFBC2_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_bfbc2_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_bfbc2_request_packet( struct qserver *server );

#endif

