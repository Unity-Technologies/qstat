/*
 * qstat 2.11
 * by Steve Jankowski
 *
 * Teeworlds protocol
 * Copyright 2008 ? Emiliano Leporati
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TEE_H
#define QSTAT_TEE_H

#include "qserver.h"

// Packet processing methods
int deal_with_tee_packet( struct qserver *server, char *pkt, int pktlen );
int send_tee_request_packet( struct qserver *server );

#endif

