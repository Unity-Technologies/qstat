/*
 * qstat
 * by Steve Jankowski
 *
 * Teeworlds protocol
 * Thanks to Emiliano Leporati for the first Teeworlds server patch (2008)
 * Thanks to Thomas Debesse for the server rewrite and master addition <dev@illwieckz.net> (2014-2016)
 * Thanks to Steven Hartland for some parts and generous help <steven.hartland@multiplay.co.uk> (2014)
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

/* See "scripts/tw_api.py" from Teeworlds project */

/* query server */
static char teeserver_request_packet[15] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'g', 'i', 'e', '\x00', '\x00' };
static int len_teeserver_request_packet = sizeof(teeserver_request_packet) / sizeof(char);

/* server response */
static char teeserver_info_headerprefix[13] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f' };
static int len_teeserver_info_headerprefix = sizeof(teeserver_info_headerprefix) / sizeof(char);

/*
 * To request, we will try 3 request packets, only one character and the size differ, so no need to declare 3 strings
 *
 * char teeserver_request_packet[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'g', 'i', 'e', 'f' };
 * char teeserver_request_packe2[15] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'g', 'i', 'e', '2', '\x00' };
 * char teeserver_request_packe3[15] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'g', 'i', 'e', '3', '\x00' };
 *
 * To analyze response, we will compare the same string without the last character, then the last character, so no need to declare 3 strings
 *
 * char teeserver_info_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', 'o' };
 * char teeserver_inf2_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', '2' };
 * char teeserver_inf3_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', '3' };
 */

/*
 * For information, Teeworlds packet samples per header, explained
 *
 * Header "info": \xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', 'o'
 * Answer sample: '\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffinfo0.5.1\x00.:{TeeKnight|Catch16}:. Catch16 hosted by TeeKnight.de\x00lightcatch\x00Catch16\x000\x00-1\x000\x0016\x00'
 * Answer format: (*char)header,(*char)version,(*char)name,(*char)map,(*char)gametype,(int)flags,(int)progression,(int)num_players,(int)max_players,*players[]
 *
 * Header "inf2": \xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', '2'
 * Answer sample: '\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffinf20\x000.5.1\x00.:{TeeKnight|Catch16}:. Catch16 hosted by TeeKnight.de\x00lightcatch\x00Catch16\x000\x00-1\x000\x0016\x00'
 * Answer format: (*char)header,(*char)token,(*char)version),(*char)name,(*char)map,(*char)gametype,(int)flags,(int)progression,(int)num_players,(int)max_players,*players[]
 *
 * Header "inf3": \xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', '3'
 * Answer sample: '\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffinf30\x000.6.1\x00[Bigstream.ru] CTF5 only\x00ctf5\x00CTF\x000\x000\x0016\x000\x0016\x00'
 * Answer format: (*char)header,(*char)token,(*char)version),(*char)name,(*char)map,(*char)gametype,(int)flags,(int)num_players,(int)max_players,(int)num_clients,(int)max_clients,*players[]
 */

/* query master */
static char teemaster_packet[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', 't' };
static int len_teemaster_packet = sizeof(teemaster_packet) / sizeof(char);

/* master response header, the last char defines the protocol version, everything before that last char is the same */
static char teemaster_list_headerprefix[13] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's' };
static int len_teemaster_list_headerprefix = sizeof(teemaster_list_headerprefix) / sizeof(char);
static int len_teemaster_list_header = 1 + sizeof(teemaster_list_headerprefix) / sizeof(char);

/*
 * To request, we will try 2 request packets, only one character differs, so no need to declare 2 strings
 *
 * char teemaster_packet[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', 't' };
 * char teemaster_packe2[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', '2' };
 *
 * To analyze response, we will compare the same string without the last character, so no need to declare 3 strings
 *
 * char teemaster_list_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's', 't' };
 * char teemaster_lis2_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's', '2' };
 */


/* ipv4 header for ipv4 addresses stored in ipv6 slots in normal server list */
static char tee_ipv4_header[12] = { '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF' };
static int len_tee_ipv4_header = sizeof(tee_ipv4_header) / sizeof(char);

query_status_t
send_teeserver_request_packet(struct qserver *server)
{
	debug(2, "send_teeserver_request_packet %p", server);

	query_status_t ret;

	/*
	 * Try first then second then third...
	 * In fact the master server said which server use which protocol, but how qstat can transmit this information?
	 */

	/* send getinfo1 packet (legacy server first query) */
	teeserver_request_packet[13] = 'f';
	ret = send_packet(server, teeserver_request_packet, 13);
	if (ret != INPROGRESS) {
		return (ret);
	}

	/* send getinfo2 packet (legacy server second query for additional data) */
	teeserver_request_packet[13] = '2';
	ret = send_packet(server, teeserver_request_packet, len_teeserver_request_packet);
	if (ret != INPROGRESS) {
		return (ret);
	}

	/* send getinfo3 packet (normal server lone query) */
	teeserver_request_packet[13] = '3';
	return (send_packet(server, teeserver_request_packet, len_teeserver_request_packet));
}


query_status_t
deal_with_teeserver_packet(struct qserver *server, char *rawpkt, int rawpktlen)
{
	int i;
	char last_char;
	char *current = NULL, *end = NULL, *version = NULL, *tok = NULL;
	struct player *player;

	debug(2, "deal_with_teeserver_packet %p, %d", server, rawpktlen);

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	if (rawpktlen < len_teeserver_info_headerprefix) {
		malformed_packet(server, "packet too short");
		return (PKT_ERROR);
	}

	/* not null-terminated packet */
	if ((strnlen(rawpkt, rawpktlen) == rawpktlen) && (rawpkt[rawpktlen] != 0)) {
		malformed_packet(server, "not null-terminated packet");
		return (PKT_ERROR);
	}

	/* get the last character */
	last_char = rawpkt[len_teeserver_info_headerprefix];

	/* compare the response without the last character */
	if (memcmp(rawpkt, teeserver_info_headerprefix, len_teeserver_info_headerprefix) != 0) {
		malformed_packet(server, "unknown packet header");
		return (PKT_ERROR);
	}

	/* and verify if the last character is 'o', '2' or '3' */
	if ((last_char != 'o') && (last_char != '2') && (last_char != '3')) {
		malformed_packet(server, "unknown packet format");
		return (PKT_ERROR);
	}

	current = rawpkt;
	end = rawpkt + rawpktlen;

	/* header, skip */
	current += len_teeserver_info_headerprefix + sizeof(last_char);

	/* if inf2 or inf3 */
	if ((last_char == '2') || (last_char == '3')) {
		/* token, skip */
		current += strnlen(current, end - current) + 1;
	}

	/* version */
	version = current;
	current += strnlen(current, end - current) + 1;

	/* server name */
	server->server_name = strdup(current);
	current += strnlen(current, end - current) + 1;

	/* map name */
	server->map_name = strdup(current);
	current += strnlen(current, end - current) + 1;

	/* game type */
	add_rule(server, server->type->game_rule, current, NO_FLAGS);
	current += strnlen(current, end - current) + 1;

	/* flags, skip */
	current += strnlen(current, end - current) + 1;

	/* if info or inf2 */
	if ((last_char == 'o') || (last_char == '2')) {
		// progression, skip
		current += strnlen(current, end - current) + 1;
	}

	/* num players */
	server->num_players = atoi(current);
	current += strnlen(current, end - current) + 1;

	/* max players */
	server->max_players = atoi(current);
	current += strnlen(current, end - current) + 1;

	/* if inf3 */
	if (last_char == '3') {
		/* is there a difference between a Teeworlds spectator and what qstat calls a "client"? */

		/* num clients, skip */
		current += strnlen(current, end - current) + 1;

		/* max clients, skip */
		current += strnlen(current, end - current) + 1;
	}

	/* players */
	for (i = 0; i < server->num_players; i++) {
		player = add_player(server, i);
		player->name = strdup(current);
		current += strnlen(current, end - current) + 1;

		player->score = atoi(current);
		current += strnlen(current, end - current) + 1;
	}

	/* version reprise */
	server->protocol_version = 0;

	tok = strtok(version, ".");
	if (tok == NULL) {
		malformed_packet(server, "malformed server version string");
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x000F) << 12;

	tok = strtok(NULL, ".");
	if (tok == NULL) {
		malformed_packet(server, "malformed server version string");
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x000F) << 8;

	tok = strtok(NULL, ".");
	if (tok == NULL) {
		malformed_packet(server, "malformed server version string");
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x00FF);

	return (DONE_FORCE);
}


query_status_t
send_teemaster_request_packet(struct qserver *server)
{
	debug(2, "send_teemaster_request_packet %p", server);

	query_status_t ret_packet, ret_packe2;

	/* query for legacy list (ipv4 only list) of legacy servers (using legacy getinfo and getinfo2 queries) */
	teemaster_packet[13] = 't';
	ret_packet = send_packet(server, teemaster_packet, len_teemaster_packet);

	if (ret_packet != INPROGRESS) {
		return (ret_packet);
	}

	/* query for normal list (mixed ipv4 and ipv6 list) of normal servers (using normal getinfo3 query) */
	teemaster_packet[13] = '2';
	ret_packe2 = send_packet(server, teemaster_packet, len_teemaster_packet);

	return (ret_packe2);
}

query_status_t
process_legacy_teemaster_packet_data(struct qserver *server, char *rawpkt, int rawpktlen)
{
	int num_servers, previous_len, len_address_packet, i;
	char *current, *dumb_pointer;

	/* six bytes address */
	debug(1, "teeworlds master response uses legacy packet format (ipv4 server list)");

	server->server_name = MASTER;
	/* length of an address packet in rawpktlen with legacy packet format */
	len_address_packet = 6;

	num_servers = rawpktlen / len_address_packet;
	server->n_servers += num_servers;

	previous_len = server->master_pkt_len;
	server->master_pkt_len = server->n_servers * 6;
	dumb_pointer = (char *)realloc(server->master_pkt, server->master_pkt_len);
	if (dumb_pointer == NULL) {
		free(server->master_pkt);
		debug(0, "Failed to realloc memory for internal master packet");
		return (MEM_ERROR);
	}
	server->master_pkt = dumb_pointer;
	current = server->master_pkt + previous_len;

	for (i = 0; i < num_servers; i++) {
		if (rawpktlen < len_address_packet) {
			malformed_packet(server, "packet too short");
			return (PKT_ERROR);
		}

		memcpy(current, rawpkt, 4);
		memcpy(current + 5, rawpkt + 4, 1);
		memcpy(current + 4, rawpkt + 5, 1);
		/* 6 is the internal length of an address in qstat's server->master_pkt */
		current += 6;

		rawpkt += len_address_packet;
		rawpktlen -= len_address_packet;
	}

	return (INPROGRESS);
}

query_status_t
process_new_teemaster_packet_data(struct qserver *server, char *rawpkt, int rawpktlen)
{
	int num_servers, previous_len, len_address_packet, i;
	char *current, *dumb_pointer;

	/* eighteen bytes address */
	debug(1, "teeworlds master response uses normal packet format (mixed ipv4/ipv6 server list)");

	server->server_name = MASTER;
	len_address_packet = 18;

	num_servers = rawpktlen / len_address_packet;

	for (i = 0; i < num_servers; i++) {
		if (rawpktlen < len_address_packet) {
			malformed_packet(server, "packet too short");
			return (PKT_ERROR);
		}

		if (memcmp(rawpkt, tee_ipv4_header, len_tee_ipv4_header) == 0) {
			debug(3, "teeworlds ipv4 server found in normal teeworlds master packet");

			previous_len = server->master_pkt_len;
			server->n_servers += 1;
			server->master_pkt_len += 6;
			dumb_pointer = (char *)realloc(server->master_pkt, server->master_pkt_len);
			if (dumb_pointer == NULL) {
				free(server->master_pkt);
				debug(0, "Failed to realloc memory for internal master packet");
				return (MEM_ERROR);
			}
			server->master_pkt = dumb_pointer;
			current = server->master_pkt + previous_len;

			memcpy(current, rawpkt + len_tee_ipv4_header, 6);

		} else {
			/* currently unsuported */
			debug(3, "teeworlds ipv6 server found in normal teeworlds master packet, discarding");
		}

		rawpkt += len_address_packet;
		rawpktlen -= len_address_packet;
	}

	return (INPROGRESS);
}

query_status_t
deal_with_teemaster_packet(struct qserver *server, char *rawpkt, int rawpktlen)
{
	char last_char;

	debug(2, "deal_with_teemaster_packet %p, %d", server, rawpktlen);

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	if (rawpktlen < len_teemaster_list_headerprefix) {
		malformed_packet(server, "packet too short");
		return (PKT_ERROR);
	}

	/* compare the response without the last character */
	if (memcmp(rawpkt, teemaster_list_headerprefix, len_teemaster_list_headerprefix) != 0) {
		malformed_packet(server, "unknown packet header");
		return (PKT_ERROR);
	}

	/* get the last header character */
	last_char = rawpkt[len_teemaster_list_headerprefix];

	/* jump to the data */
	rawpkt += len_teemaster_list_header;
	rawpktlen -= len_teemaster_list_header;

	if (last_char == 't') {
		return process_legacy_teemaster_packet_data(server, rawpkt, rawpktlen);
	} else if (last_char == '2') {
		return process_new_teemaster_packet_data(server, rawpkt, rawpktlen);
	}

	/* unknown server list format */
	malformed_packet(server, "unknown packet format");
	return (PKT_ERROR);
}
