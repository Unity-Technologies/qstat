/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Teamspeak 2 protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_GPS_H
#define QSTAT_GPS_H

#include "qserver.h"

// Packet processing methods
void deal_with_ts2_packet( struct qserver *server, char *pkt, int pktlen );
void send_ts2_request_packet( struct qserver *server );

#endif

