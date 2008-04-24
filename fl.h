/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Frontlines-Fuel of War protocol
 * Copyright 2008 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_FL_H
#define QSTAT_FL_H

#include "qserver.h"

int send_fl_request_packet(struct qserver *server);
int send_fl_rule_request_packet(struct qserver *server);
int deal_with_fl_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
