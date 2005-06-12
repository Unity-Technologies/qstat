/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Half-Life2 query protocol
 * Copyright 2005 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_A2S_H
#define QSTAT_A2S_H

#include "qserver.h"

void send_a2s_request_packet(struct qserver *server);
void send_a2s_rule_request_packet(struct qserver *server);
void deal_with_a2s_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
