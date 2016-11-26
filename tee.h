/*
 * qstat
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
query_status_t send_teeserver_request_packet(struct qserver *server);
query_status_t deal_with_teeserver_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif

