/*
 * qstat
 * by Steve Jankowski
 *
 * Gamespy query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GPS_H
#define QSTAT_GPS_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_gps_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_gps_request_packet( struct qserver *server );

#endif

