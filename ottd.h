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

int send_ottdmaster_request_packet(struct qserver *server);
int deal_with_ottdmaster_packet(struct qserver *server, char *rawpkt, int pktlen);

int send_ottd_request_packet(struct qserver *server);
int deal_with_ottd_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
