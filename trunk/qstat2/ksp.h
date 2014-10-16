/*
 * qstat
 * by Steve Jankowski
 *
 * KSP query protocol
 * Copyright 2014 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_KSP_H
#define QSTAT_KSP_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_ksp_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_ksp_request_packet( struct qserver *server );

#endif

