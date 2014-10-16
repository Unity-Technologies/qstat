/*
 * qstat
 * by Steve Jankowski
 *
 * Cube 2 / Sauerbraten protocol
 * Copyright 2011 NoisyB
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_CUBE2_H
#define QSTAT_CUBE2_H


#include "qserver.h"

// Packet processing methods
query_status_t deal_with_cube2_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_cube2_request_packet( struct qserver *server );


#endif  // QSTAT_CUBE2_H


