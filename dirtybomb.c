/*
 * qstat
 * by Steve Jankowski
 *
 * DirtyBomb query protocol
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


int unpack_msgpack(struct qserver *server, unsigned char *, unsigned char *);

query_status_t
send_dirtybomb_request_packet(struct qserver *server)
{
	char buf[1024], *password, chunks, len;

	password = get_param_value(server, "password", "");
	len = strlen(password);
	chunks = 0x01;
	if (server->flags & TF_RULES_QUERY) {
		chunks |= 0x02;
	}
	if (server->flags & TF_PLAYER_QUERY) {
		chunks |= 0x04; // Player
		chunks |= 0x08; // Team - Currently not supported
	}
	sprintf(buf, "%c%c%s%c", 0x01, len, password, chunks);

	server->saved_data.pkt_max = -1;

	return send_packet(server, buf, len + 3);
}

query_status_t
deal_with_dirtybomb_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned char *s, *l, pkt_id, pkt_max;

	debug(2, "processing...");
	
	if (8 > pktlen) {
		// not valid response
		malformed_packet(server, "packet too small");
		return PKT_ERROR;
	}

	if (rawpkt[4] != 0x01) {
		// not a known response
		malformed_packet(server, "unknown packet type 0x%02hhx", rawpkt[4]);
		return PKT_ERROR;
	}

	// Response ID - long
	// Response Type - byte
	// Current Packet Number - byte
	pkt_id = rawpkt[5];

	// Last Packet Number - byte
	pkt_max = rawpkt[6];

	// Payload Blob - msgpack map
	s = (unsigned char*)rawpkt + 7;
	
	if (!server->combined) {
		int pkt_cnt = packet_count(server);

		server->retry1 = n_retries;
		if (0 == server->n_requests) {
			server->ping_total = time_delta( &packet_recv_time, &server->packet_time1 );
			server->n_requests++;
		}

		if (pkt_cnt < pkt_max) {
			// We're expecting more to come
			debug( 5, "fragment recieved..." );

			if (!add_packet(server, 0, pkt_id, pkt_max, pktlen, rawpkt, 1)) {
				// fatal error e.g. out of memory
				return MEM_ERROR;
			}

			// combine_packets will call us recursively
			return combine_packets(server);
		}
	}

	// Correct ping
	// Not quite right but gives a good estimate
	server->ping_total = (server->ping_total * server->n_requests) / 2;

	debug(3, "processing response...");

	gettimeofday(&server->packet_time1, NULL);

	l = (unsigned char *)rawpkt + pktlen - 1;
	if (!unpack_msgpack(server, s, l)) {
		return PKT_ERROR;
	}

	return DONE_FORCE;
}

int
unpack_msgpack_pos_fixnum(struct qserver *server, uint8_t *val, unsigned char **datap, unsigned char *last, int raw)
{


	if (!raw) {
		if (*datap > last) {
			malformed_packet(server, "packet too small");
			return 0;
		}

		if ((**datap & 0x80) != 0) {
			malformed_packet(server, "invalid positive fixnum type 0x%02hhx", **datap);
			return 0;
		}

	}

	*val = **datap;
	(*datap)++;

	return 1;
}

int
unpack_msgpack_uint16(struct qserver *server, uint16_t *val, unsigned char **datap, unsigned char *last, int raw)
{


	if (!raw) {
		if (*datap > last) {
			malformed_packet(server, "packet too small");
			return 0;
		}

		if (**datap != 0xcd) {
			malformed_packet(server, "invalid uint16 type 0x%02hhx", **datap);
			return 0;
		}

		(*datap)++;
	}

	if (*datap + 2 > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}
	*val = (uint16_t)(*datap)[1] | ((uint16_t)(*datap)[0] << 8);
	*datap += 2;

	return 1;
}

int
unpack_msgpack_uint32(struct qserver *server, uint32_t *val, unsigned char **datap, unsigned char *last, int raw)
{


	if (!raw) {
		if (*datap > last) {
			malformed_packet(server, "packet too small");
			return 0;
		}

		if (**datap != 0xce) {
			malformed_packet(server, "invalid uint32 type 0x%02hhx", **datap);
			return 0;
		}

		(*datap)++;
	}

	if (*datap + 4 > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}

	*val = (uint32_t)(*datap)[3] |
		((uint32_t)(*datap)[2] << 8) |
		((uint32_t)(*datap)[1] << 16) |
		((uint32_t)(*datap)[0] << 24);
	*datap += 2;

	return 1;
}

int
unpack_msgpack_raw_len(struct qserver *server, char **valp, unsigned char **datap, unsigned char *last, uint32_t len)
{
	char *data;

	debug(4, "raw_len: 0x%lu\n", (unsigned long)len);

	if (*datap + len > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}

	if (NULL == (data = malloc(len + 1))) {
		malformed_packet(server, "out of memory");
		return 0;
	}

	memcpy(data, *datap, len);
	data[len] = '\0';

	*valp = data;
	(*datap) += len;

	return 1;
}

int
unpack_msgpack_raw(struct qserver *server, char **valp, unsigned char **datap, unsigned char *last)
{
	unsigned char type, len;

	if (*datap + 1 > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}

	type = (**datap & 0xa0);
	len = (**datap & 0x1f);
	if (0xa0 != type) {
		malformed_packet(server, "invalid raw type 0x%02hhx", **datap);
		return 0;
	}
	(*datap)++;

	return unpack_msgpack_raw_len(server, valp, datap, last, (uint32_t)len);
}

int
unpack_msgpack_raw16(struct qserver *server, char **valp, unsigned char **datap, unsigned char *last)
{
	uint16_t len;

	if (*datap + 3 > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}
	(*datap)++;

	if (!unpack_msgpack_uint16(server, &len, datap, last, 1))
		return 0;

	return unpack_msgpack_raw_len(server, valp, datap, last, (uint32_t)len);
}

int
unpack_msgpack_raw32(struct qserver *server, char **valp, unsigned char **datap, unsigned char *last)
{
	uint32_t len;

	if (*datap + 5 > last) {
		malformed_packet(server, "packet too small");
		return 0;
	}
	(*datap)++;

	if (!unpack_msgpack_uint32(server, &len, datap, last, 1))
		return 0;

	return unpack_msgpack_raw_len(server, valp, datap, last, len);
}

int
unpack_msgpack_string(struct qserver *server, char **valp, unsigned char **datap, unsigned char *last)
{
	unsigned char type = **datap;

	debug(4, "string: 0x%02hhx\n", type);

	if (0xa0 == (type & 0xa0))
		return unpack_msgpack_raw(server, valp, datap, last);
	else if (type == 0xda)
		return unpack_msgpack_raw16(server, valp, datap, last);
	else if (type == 0xdb)
		return unpack_msgpack_raw32(server, valp, datap, last);
	else
		malformed_packet(server, "invalid string type 0x%02hhx", type);

	return 0;
}

int
unpack_msgpack(struct qserver *server, unsigned char *s, unsigned char *l)
{
	uint16_t elements;
	unsigned char type, type_len;
	char *var, *str_val;
	uint8_t uint8_val;

	type_len = *s;
	s++;
	debug(3, "type/len: 0x%02hhx\n", type_len);
	if (0x80 == (type_len & 0x80)) {
		type = (type_len & 0x80);
		elements = (type_len & 0x0f);
		debug(3, "map type: 0x%02hhx, elements: %d\n", type, elements);

	} else if (type_len == 0xde) {
		// map 16
		if (!unpack_msgpack_uint16(server, &elements, &s, l, 1))
			return 0;
	} else {
		// There is a map 32 but we don't support it
		malformed_packet(server, "invalid map type 0x%02hhx", type_len);
		return 0;
	}

	while(elements) {
		type = *s;
		if (!unpack_msgpack_string(server, &var, &s, l))
			return 0;

		debug(4, "Map[%s]\n", var);

		if (0 == strcmp(var, "SN")) {
			// Server Name
			if (!unpack_msgpack_string(server, &str_val, &s, l))
				return 0;
			server->server_name = str_val;

		} else if (0 == strcmp(var, "MAP")) {
			// Map
			if (!unpack_msgpack_string(server, &str_val, &s, l))
				return 0;
			server->map_name = str_val;

		} else if (0 == strcmp(var, "MP")) {
			// Max Players
			if (!unpack_msgpack_pos_fixnum(server, &uint8_val, &s, l, 0))
				return 0;
			server->max_players = uint8_val;

		} else if (0 == strcmp(var, "NP")) {
			// Number of Players
			if (!unpack_msgpack_pos_fixnum(server, &uint8_val, &s, l, 0))
				return 0;
			server->num_players = uint8_val;

		} else if (0 == strcmp(var, "RL")) {
			// Gametype
			if (!unpack_msgpack_string(server, &str_val, &s, l))
				return 0;

			server->game = str_val;
			add_rule(server, "gametype", str_val, NO_FLAGS);

		} else if (0 == strcmp(var, "SFT")) {
			// Current Frametime in ms
			if (!unpack_msgpack_pos_fixnum(server, &uint8_val, &s, l, 0))
				return 0;

		} else {
			debug(4, "Unknown setting '%s'\n", var);
			s++;
		}

		free(var);
		elements--;
	}

	return 1;
}
