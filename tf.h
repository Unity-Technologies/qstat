/*
 * qstat
 *
 * Titafall protocol
 * Copyright 2015 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_TF_H
#define QSTAT_TF_H

#include "qserver.h"

query_status_t send_tf_request_packet(struct qserver *server);
query_status_t deal_with_tf_packet(struct qserver *server, char *rawpkt, int pktlen);

#endif
