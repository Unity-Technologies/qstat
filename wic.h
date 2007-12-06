/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * World in Conflict Protocol
 * Copyright 2007 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GPS_H
#define QSTAT_GPS_H

#include "qserver.h"

// Packet processing methods
void deal_with_wic_packet( struct qserver *server, char *pkt, int pktlen );
void send_wic_request_packet( struct qserver *server );

#endif

