/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * Crysis query protocol
 * Copyright 2012 Steven Hartland
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

char *decode_farmsim_val(char *val)
{
	// Very basic html conversion
	val = str_replace(val, "&quot;", "\"");
	return str_replace(val, "&amp;", "&");
}

query_status_t send_farmsim_request_packet(struct qserver *server)
{
	char buf[256], *code;

	server->saved_data.pkt_max = -1;
	code = get_param_value(server, "code", "");
	sprintf(buf, "GET /feed/dedicated-server-stats.xml?code=%s HTTP/1.1\015\012User-Agent: qstat\015\012\015\012", code);

	return send_packet(server, buf, strlen(buf));
}

query_status_t valid_farmsim_response(struct qserver *server, char *rawpkt, int pktlen)
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

char *farmsim_xml_attrib(char *line, char *name)
{
	char *q, *p, *val;

	p = strstr(line, name);
	if (p == NULL) {
		return NULL;
	}

	p += strlen(name);
	if (strlen(p) < 4) {
		return NULL;
	}
	p += 2;

	q = strchr(p, '"');
	if (q == NULL) {
		return NULL;
	}
	*q = '\0';
	
	val = strdup(p);
	*q = '"';
	debug(4, "%s = %s", name, val);

	return val;
}

query_status_t deal_with_farmsim_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, *val, *line;
	query_status_t state = INPROGRESS;
	debug(2, "processing...");

	if (!server->combined) {
		state = valid_farmsim_response(server, rawpkt, pktlen);
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
		state = valid_farmsim_response(server, rawpkt, pktlen);
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
	s = decode_farmsim_val(s);
	line = strtok(s, "\012");

	// NOTE: id=XXX and msg=XXX will be processed by the mod following the one they where the response of
	while (line != NULL) {
		debug(4, "LINE: %s\n", line);
		if (strstr(line, "<Server") != NULL) {
			debug(1, "<Server...");
			// <Server
			// game="Farming Simulator 2013"
			// version="2.0.0.5 Public Beta 3"
			// server="International"
			// name="Multiplay :: liv3dz0r"
			// mapName="Hagenstedt"
			// money="5993"
			// dayTime="24687375"
			// mapOverviewFilename="data/maps/map01/pda_map.png">

			// Server Name
			val = farmsim_xml_attrib(line, "name");
			if (val != NULL) {
				server->server_name = val;
			} else {
				server->server_name = strdup("Unknown");
			}

			// Map Name
			val = farmsim_xml_attrib(line, "mapName");
			if (val != NULL) {
				server->map_name = val;
			} else {
				server->map_name = strdup("Default");
			}
		} else if (strstr(line, "<Slots") != NULL) {
			// <Slots capacity="16" numUsed="1">
			debug(1, "<Slots...");

			// Max Players
			val = farmsim_xml_attrib(line, "capacity");
			if (val != NULL) {
				server->max_players = atoi(val);
				free(val);
			} else {
				server->max_players = get_param_ui_value(server, "maxplayers", 1);
			}
			// Num Players
			val = farmsim_xml_attrib(line, "numUsed");
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

