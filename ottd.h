/*
 * qstat 2.11
 *
 * opentTTD protocol
 * Copyright 2007 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_OTTD_H
#define QSTAT_OTTD_H

#include "qstat.h"

query_status_t send_ottdmaster_request_packet(struct qserver *server);
query_status_t deal_with_ottdmaster_packet(struct qserver *server, char *rawpkt, int pktlen);

query_status_t send_ottd_request_packet(struct qserver *server);
query_status_t deal_with_ottd_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
