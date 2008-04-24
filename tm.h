/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * TrackMania protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GPS_H
#define QSTAT_GPS_H

#include "qserver.h"

// Packet processing methods
int deal_with_tm_packet( struct qserver *server, char *pkt, int pktlen );
int send_tm_request_packet( struct qserver *server );

#endif

