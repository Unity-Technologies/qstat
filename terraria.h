/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Terraria / TShock protocol
 * Copyright 2012 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TERRARIA_H
#define QSTAT_TERRARIA_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_terraria_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_terraria_request_packet( struct qserver *server );

#endif

