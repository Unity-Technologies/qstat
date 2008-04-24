/*
 * qstat 2.6
 * by Steve Jankowski
 *
 * debug helper functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_UT2004_H
#define QSTAT_UT2004_H

#include "qstat.h"

int send_ut2004master_request_packet(struct qserver *server);
int deal_with_ut2004master_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
