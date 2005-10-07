/*
 * qstat 2.9
 * by Steve Jankowski
 *
 * Doom3 protocol
 * Copyright 2005 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_DOOM3_H
#define QSTAT_DOOM3_H

#include "qstat.h"

char* build_doom3_masterfilter(struct qserver* server, char* buf, unsigned* buflen);
void deal_with_doom3_packet( struct qserver *server, char *rawpkt, int pktlen);

#endif
