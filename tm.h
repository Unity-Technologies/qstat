/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * TrackMania protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TM_H
#define QSTAT_TM_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_tm_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_tm_request_packet( struct qserver *server );

#endif

