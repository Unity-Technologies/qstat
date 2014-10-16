/*
 * qstat
 * by Steve Jankowski
 *
 * StarMade query protocol
 * Copyright 2013 Steven Hartland
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
#include "qstat.h"
#include "utils.h"
#include "packet_manip.h"

typedef enum {
	SM_INT = 0x01,
	SM_LONG,
	SM_FLOAT,
	SM_STRING,
	SM_BOOLEAN,
	SM_BYTE,
	SM_SHORT,
	SM_BYTE_ARRAY
} starmade_param_type;


query_status_t
send_starmade_request_packet(struct qserver *server)
{
	char buf[13] = { 0x00, 0x00, 0x00, 0x09, 0x2a, 0xff, 0xff, 0x01, 0x6f, 0x00, 0x00, 0x00, 0x00 };

	/*
	 * Header
	 * int(4)	- packet length
	 * byte		- packet type : 0x2a = packet, 0x20 = ping, test = 0x64, logout = 0x41
	 * short(2)	- packet id
	 * byte		- command id : 0x01 = status, 0x02 = exec
	 * byte		- command type : 0x6f = parameterised command, 0x84 = stream command
	 *
	 * Paramters
	 * int(4)	- paramter count
	 * byte		- paramter type : 1 = int, 2 = long, 3 = float, 4 = UTF8 string, 5 = boolean, 6 = byte, 7 = short, 8 = byte array
	 * ...
	 */

	debug(3, "send_starmade_request_packet: state = %ld", server->challenge);

	return send_packet(server, buf, 13);
}

static query_status_t
starmade_read_parameter(char **datap, int *datalen, void *val, int vlen, starmade_param_type type)
{
	int size;

	if (**datap != type) {
		/* unexpected type */
		debug(2, "Invalid type detected, expected 0x%02x got 0x%02x", type, **datap);
		return REQ_ERROR;
	}

	(*datap)++;
	(*datalen)--;

	switch(type) {
	case SM_INT:
		size = 4;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(uint32_t*)val = ((uint32_t)(*datap)[3]) |
			((uint32_t)(*datap)[2] << 8) |
			((uint32_t)(*datap)[1] << 16) |
			((uint32_t)(*datap)[0] << 24);
		break;
	case SM_LONG:
		size = 8;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(uint64_t*)val = ((uint64_t)(*datap)[7]) |
			((uint64_t)(*datap)[6] << 8) |
			((uint64_t)(*datap)[5] << 16) |
			((uint64_t)(*datap)[4] << 24) |
			((uint64_t)(*datap)[3] << 32) |
			((uint64_t)(*datap)[2] << 40) |
			((uint64_t)(*datap)[1] << 48) |
			((uint64_t)(*datap)[0] << 56);
		break;
	case SM_FLOAT:
		size = 4;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(uint32_t*)val = (uint32_t)(*datap)[3] |
			((uint32_t)(*datap)[2] << 8) |
			((uint32_t)(*datap)[1] << 16) |
			((uint32_t)(*datap)[0] << 24);
		break;
	case SM_STRING:
		size = ((short)(*datap)[1]) | ((short)(*datap)[0] << 8);
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		(*datap) += 2;
		memcpy(val, *datap, size);
		((char*)val)[size] = 0x00;
		break;
	case SM_BOOLEAN:
		size = 1;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(unsigned char*)val = **datap;
		break;
	case SM_BYTE:
		size = 1;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(unsigned char*)val = **datap;
		break;
	case SM_SHORT:
		size = 2;
		if (size > vlen || size > *datalen) {
			return MEM_ERROR;
		}
		*(short*)val = (short)(*datap)[1] | ((short)(*datap)[0] << 8);
		break;
	case SM_BYTE_ARRAY:
		debug(2, "Unsupport type 0x%02x requested / received", type);
		return REQ_ERROR;
	default:
		debug(2, "Unknown type 0x%02x requested / received", type);
		return REQ_ERROR;
	}

	(*datap) += size;
	(*datalen) -= size;

	return INPROGRESS;
}

query_status_t
deal_with_starmade_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, buf[256];
	int ret;

	debug(2, "processing...");

	if (pktlen < 54) {
		// Invalid password
		return REQ_ERROR;
	}

	s = rawpkt;

	// TODO: continue reading / combining packets until we've read enough.
	debug(3, "packet: combined = %d, challenge = %ld", server->combined, server->challenge);
	// Packet Header
	// int - response size excluding header (12 bytes)
	s += 4;
	// long - ts??
	s += 8;

	// Response Header
	// TODO: validate header details
	// byte - packet type
	s += 1;
	// short - packet id
	s += 2;
	// byte - command id
	s += 1;
	// byte - command type
	s += 1;

	// Parameters
	// int - paramater count
	s += 4;

	// byte - info version
	ret = starmade_read_parameter(&s, &pktlen, buf, sizeof(buf), SM_BYTE);
	if (ret != INPROGRESS)
		return ret;

	// float - version
	ret = starmade_read_parameter(&s, &pktlen, buf, sizeof(buf), SM_FLOAT);
	if (ret != INPROGRESS)
		return ret;

	// string - server name
	ret = starmade_read_parameter(&s, &pktlen, buf, sizeof(buf), SM_STRING);
	if (ret != INPROGRESS)
		return ret;
	server->server_name = strdup(buf);

	// string - description
	ret = starmade_read_parameter(&s, &pktlen, buf, sizeof(buf), SM_STRING);
	if (ret != INPROGRESS)
		return ret;

	// long - start time
	ret = starmade_read_parameter(&s, &pktlen, buf, sizeof(buf), SM_LONG);
	if (ret != INPROGRESS)
		return ret;

	// int - player count
	ret = starmade_read_parameter(&s, &pktlen, &server->num_players, sizeof(server->num_players), SM_INT);
	if (ret != INPROGRESS)
		return ret;

	// int - max players
	ret = starmade_read_parameter(&s, &pktlen, &server->max_players, sizeof(server->max_players), SM_INT);
	if (ret != INPROGRESS)
		return ret;

	gettimeofday(&server->packet_time1, NULL);

	server->map_name = strdup("default");

	return DONE_FORCE;
}

