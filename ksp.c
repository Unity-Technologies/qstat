/*
 * qstat
 * by Steve Jankowski
 *
 * KSP query protocol
 * Copyright 2014 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "utils.h"
#include "qstat.h"
#include "md5.h"
#include "packet_manip.h"

char *decode_ksp_val(char *val)
{
	// Very basic html conversion
	val = str_replace(val, "&quot;", "\"");
	return str_replace(val, "&amp;", "&");
}

query_status_t send_ksp_request_packet(struct qserver *server)
{
	char buf[256];

	server->saved_data.pkt_max = -1;
	sprintf(buf, "GET / HTTP/1.1\015\012User-Agent: qstat\015\012Host: %s:%d\015\012\015\012", server->host_name, server->port);

	return send_packet(server, buf, strlen(buf));
}

query_status_t valid_ksp_response(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s;
	int len;
	int cnt = packet_count(server);
	if (0 == cnt && 0 != strncmp("HTTP/1.1 200 OK", rawpkt, 15)) {
		// not valid response
		debug(2, "Invalid");
		return REQ_ERROR;
	}

	s = strnstr(rawpkt, "Content-Length: ", pktlen);
	if (s == NULL) {
		// not valid response
		debug(2, "Invalid (no content-length)");
		return INPROGRESS;
	}
	s += 16;

	// TODO: remove this bug work around
	if (*s == ':') {
		s += 2;
	}
	if (sscanf(s, "%d", &len) != 1) {
		debug(2, "Invalid (no length)");
		return INPROGRESS;
	}

	s = strnstr(rawpkt, "\015\012\015\012", pktlen);
	if (s == NULL) {
		debug(2, "Invalid (no end of header");
		return INPROGRESS;
	}

	s += 4;
	if (pktlen != (s - rawpkt + len)) {
		debug(2, "Outstanding data");
		return INPROGRESS;
	}

	debug(2, "Valid data");
	return DONE_FORCE;
}

char *ksp_json_attrib(char *line, char *name)
{
	char *q, *p, *val;

	p = strstr(line, name);
	if (p == NULL) {
		return NULL;
	}

	p += strlen(name);
	if (strlen(p) < 3) {
		return NULL;
	}
	p += 2;
	if (*p == '"') {
		// String
		p++;
		q = strchr(p, '"');
		if (q == NULL) {
			return NULL;
		}
	} else {
		// Integer, bool etc
		q = strchr(p, ',');
		if (q == NULL) {
			return NULL;
		}
	}
	*q = '\0';
	
	val = strdup(p);
	*q = '"';
	debug(4, "%s = %s", name, val);

	return val;
}

query_status_t deal_with_ksp_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, *val, *line;
	query_status_t state = INPROGRESS;
	debug(2, "processing...");

	if (!server->combined) {
		state = valid_ksp_response(server, rawpkt, pktlen);
		server->retry1 = n_retries;
		if (server->n_requests == 0) {
			server->ping_total = time_delta(&packet_recv_time, &server->packet_time1);
			server->n_requests++;
		}

		switch (state) {
		case INPROGRESS: {
			// response fragment recieved
			int pkt_id;
			int pkt_max;

			// We're expecting more to come
			debug(5, "fragment recieved...");
			pkt_id = packet_count(server);
			pkt_max = pkt_id + 1;
			if (!add_packet(server, 0, pkt_id, pkt_max, pktlen, rawpkt, 1)) {
				// fatal error e.g. out of memory
				return MEM_ERROR;
			}

			// combine_packets will call us recursively
			return combine_packets(server);
		}
		case DONE_FORCE:
			break; // single packet response fall through
		default:
			return state;
		}
	}

	if (state != DONE_FORCE) {
		state = valid_ksp_response(server, rawpkt, pktlen);
		switch (state) {
		case DONE_FORCE:
			break; // actually process
		default:
			return state;
		}
	}

	// Correct ping
	// Not quite right but gives a good estimate
	server->ping_total = (server->ping_total * server->n_requests) / 2;

	debug(3, "processing response...");

	s = rawpkt;
	// Ensure we're null terminated (will only loose the last \x0a)
	s[pktlen - 1] = '\0';
	s = decode_ksp_val(s);
	line = strtok(s, "\012");

	// NOTE: id=XXX and msg=XXX will be processed by the mod following the one they where the response of
	while (line != NULL) {
		debug(4, "LINE: %s\n", line);
		if (strstr(line, "{") != NULL) {
			debug(1, "{...");
			// {
			// "cheats":true,
			// "game_mode":"SANDBOX",
			// "lastPlayerActivity":81403,
			// "max_players":12,
			// "modControlSha":"e46569487926a3273f58e06a080b0747b0ae702ec1877906511fe2c29816528b",
			// "mod_control":1,
			// "player_count":0,
			// "players":"",
			// "port":6752,
			// "protocol_version":25,
			// "server_name":"Multiplay :: Online - Clanserver",
			// "universeSize":96576,
			// "version":"v0.1.5.6"
			// }

			// Server Name
			val = ksp_json_attrib(line, "server_name");
			if (val != NULL) {
				server->server_name = val;
			} else {
				server->server_name = strdup("Unknown");
			}

			// Map Name
			val = ksp_json_attrib(line, "mapName");
			if (val != NULL) {
				server->map_name = val;
			} else {
				server->map_name = strdup("Default");
			}

			// Max Players
			val = ksp_json_attrib(line, "max_players");
			if (val != NULL) {
				server->max_players = atoi(val);
				free(val);
			} else {
				server->max_players = get_param_ui_value(server, "max_players", 1);
			}

			// Num Players
			val = ksp_json_attrib(line, "player_count");
			if (val != NULL) {
				server->num_players = atoi(val);
				free(val);
			} else {
				server->num_players = 0;
			}
		}
		
		line = strtok(NULL, "\012");
	}

	gettimeofday(&server->packet_time1, NULL);

	return DONE_FORCE;
}

