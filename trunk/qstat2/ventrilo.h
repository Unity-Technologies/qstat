/*
 * qstat
 * by Steve Jankowski
 *
 * Ventrilo query protocol
 * Algorithm by Luigi Auriemma <www.aluigi.org> (Reversing and first version in c)
 * Copyright 2010 Michael Willigens <michael@willigens.de>
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_VENTRILO_H
#define QSTAT_VENTRILO_H

#include "qserver.h"

// Packet processing methods
query_status_t deal_with_ventrilo_packet( struct qserver *server, char *pkt, int pktlen );
query_status_t send_ventrilo_request_packet( struct qserver *server );

#endif

