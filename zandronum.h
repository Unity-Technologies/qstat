/*
 * qstat
 * 
 *
 * Zandronum protocol
 * Copyright 2018 DayenTech
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 * Currently does not include querying master server
 */
#ifndef QSTAT_ZANDRONUM_H
#define QSTAT_ZANDRONUM_H

#include "qstat.h"
#include "qserver.h"

#define ZANDRONUM_DEFAULT_PORT		10666

query_status_t send_zandronum_request_packet(struct qserver *server);
query_status_t deal_with_zandronum_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
