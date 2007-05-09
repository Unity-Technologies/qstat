/*
 * qstat 2.9
 * by Steve Jankowski
 *
 * Doom3 / Quake4 protocol
 * Copyright 2005 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_DOOM3_H
#define QSTAT_DOOM3_H

#include "qstat.h"

#define DOOM3_DEFAULT_PORT	27666
#define DOOM3_MASTER_DEFAULT_PORT	27650

#define QUAKE4_DEFAULT_PORT	28004
#define QUAKE4_MASTER_DEFAULT_PORT	27650

#define PREY_DEFAULT_PORT	27719
#define PREY_MASTER_DEFAULT_PORT	27655

#define ETQW_DEFAULT_PORT	27733

void send_doom3master_request_packet( struct qserver *server);
void deal_with_doom3_packet( struct qserver *server, char *rawpkt, int pktlen);

void send_quake4master_request_packet( struct qserver *server);
void deal_with_quake4_packet( struct qserver *server, char *rawpkt, int pktlen);

void deal_with_prey_packet( struct qserver *server, char *rawpkt, int pktlen);

void deal_with_etqw_packet( struct qserver *server, char *rawpkt, int pktlen);

#endif
