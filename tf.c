/*
 * qstat
 *
 * Titanfall Protocol
 * Copyright 2015 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include "debug.h"
#include "qstat.h"
#include "utils.h"

#include <stdio.h>

#define SERVERINFO_REQUEST 79
#define SERVERINFO_VERSION 1
#define SERVERINFO_RESPONSE 80

static char serverinfo_pkt[] = {0xFF, 0xFF, 0xFF, 0xFF, SERVERINFO_REQUEST, SERVERINFO_VERSION};

static void
pkt_inc(char **pkt, int *rem, int inc)
{
	*pkt += inc;
	*rem -= inc;
}

static query_status_t
pkt_string(struct qserver *server, char **pkt, char **str, int *rem)
{
	size_t len;

	if (*rem <= 0) {
		malformed_packet(server, "short packet");
		return (PKT_ERROR);
	}

	*str = strndup(*pkt, *rem);
	if (*str == NULL) {
		malformed_packet(server, "out of memory");
		return (MEM_ERROR);
	}

	len = strlen(*str) + 1;
	pkt_inc(pkt, rem, (int)len);

	return (INPROGRESS);
}

static query_status_t
pkt_rule(struct qserver *server, char *rule, char **pkt, int *rem)
{
	char *str;
	query_status_t ret;

	ret = pkt_string(server, pkt, &str, rem);
	if (ret < 0) {
		return (ret);
	}

	// TODO: check return when it doesn't exit on failure
	add_rule(server, rule, str, NO_VALUE_COPY);

	return (INPROGRESS);
}

query_status_t
send_tf_request_packet(struct qserver *server)
{
	return qserver_send_initial(server, serverinfo_pkt, sizeof(serverinfo_pkt));
}

query_status_t
deal_with_tf_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *pkt, *str, buf[16];
	query_status_t ret;
	uint16_t port;
	int rem, i;
	int32_t num;

	rem = pktlen;
	pkt = rawpkt;

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		server->n_requests++;
	}

	if (pktlen < 26) {
		malformed_packet(server, "packet too short");
		return (PKT_ERROR);
	}

	// Header (int32)
	if (memcmp(serverinfo_pkt, pkt, sizeof(int32_t)) != 0) {
		malformed_packet(server, "missing / invalid header");
		return (PKT_ERROR);
	}
	pkt_inc(&pkt, &rem, sizeof(int32_t));

	// Command (int8)
	debug(2, "TF type = %hhu", *pkt);
	if (*pkt != SERVERINFO_RESPONSE) {
		malformed_packet(server, "unknown type");
		return (PKT_ERROR);
	}
	pkt_inc(&pkt, &rem, sizeof(int8_t));

	// Version (int8)
	sprintf(buf, "%hhd", *pkt);
	add_rule(server, "version", buf, 0);
	pkt_inc(&pkt, &rem, sizeof(int8_t));

	// Port (uint16)
	port = (uint16_t)((unsigned char)pkt[0] | (unsigned char)pkt[1] << 8);
	sprintf(buf, "%hu", port);
	add_rule(server, "port", buf, 0);
	change_server_port(server, port, 0);
	pkt_inc(&pkt, &rem, sizeof(uint16_t));

	// Platform (string)
	ret = pkt_rule(server, "platform", &pkt, &rem);
	if (ret < 0) {
		return (ret);
	}

	// Playlist Version (string)
	ret = pkt_rule(server, "playlist_version", &pkt, &rem);
	if (ret < 0) {
		return (ret);
	}

	// Playlist Num (int32)
	num = (int32_t)(
		(unsigned char)pkt[0] |
		(unsigned char)pkt[1] << 8 |
		(unsigned char)pkt[2] << 16 |
		(unsigned char)pkt[3] << 24
	);
	sprintf(buf, "%d", num);
	add_rule(server, "playlist_num", buf, 0);
	pkt_inc(&pkt, &rem, sizeof(int32_t));

	// Playlist Name (string)
	ret = pkt_rule(server, "playlist_name", &pkt, &rem);
	if (ret < 0) {
		return (ret);
	}

	// Num Clients (uint8)
	server->num_players = (uint8_t)pkt[0];

	// Max Clients (uint8)
	server->max_players = (uint8_t)pkt[1];
	pkt_inc(&pkt, &rem, sizeof(uint8_t) * 2);

	// Map (string)
	ret = pkt_string(server, &pkt, &str, &rem);
	if (ret < 0) {
		return (ret);
	}
	server->map_name = str;

	// Clients
	for (i = 0; i < server->num_players; i++) {
		struct player *p;

		p = add_player(server, server->n_player_info);
		if (p == NULL) {
			// Should never happen
			return (SYS_ERROR);
		}

		// Client ID (int64)
		pkt_inc(&pkt, &rem, sizeof(int64_t));

		// Client Name (string)
		ret = pkt_string(server, &pkt, &p->name, &rem);
		if (ret < 0) {
			return (ret);
		}

		// Client Team ID (uint8)
		p->team = (uint8_t)*pkt;
		pkt_inc(&pkt, &rem, sizeof(uint8_t));

	}

	// EOP (int64)
	if (rem != 8) {
		malformed_packet(server, "packet too short");
		return (PKT_ERROR);
	}

	// Protocol doesn't support server name
	server->server_name = strdup("unknown");

	// Protocol doesn't support a specific rule request
	server->next_rule = NULL;

	return (DONE_AUTO);
}

// vim: sw=4 ts=4 noet
