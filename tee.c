/*
 * qstat
 * by Steve Jankowski
 *
 * Teeworlds protocol
 * Thanks to Emiliano Leporati for the first Teeworlds patch (2008)
 * Thanks to Thomas Debesse for the rewrite <dev@illwieckz.net> (2014)
 * Thanks to Steven Hartland for some parts and help <steven.hartland@multiplay.co.uk>
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
int len_teeserver_request_packet = 15;
char teeserver_request_packet[15] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'g', 'i', 'e', '\x00', '\x00' };

/* server response */
int len_teeserver_info_headerprefix = 13;
char teeserver_info_headerprefix[13] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f' };

/*
 * To request, we will try 3 request packet, only one character and the size change, so no need to declare 3 strings
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
 * For information, Tee packet samples per header, explained
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

int len_teemaster_packet = 14;
char teemaster_packet[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', 't' };

/* master response */
int len_teemaster_list_header = 14;
int len_teemaster_list_headerprefix = 13;
char teemaster_list_headerprefix[13] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's' };

/*
 * To request, we will try 2 request packet, only one character change, so no need to declare 2 strings
 *
 * char teemaster_packet[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', 't' };
 * char teemaster_packe2[14] = { '\x20', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF', '\xFF', '\xFF', 'r', 'e', 'q', '2' };
 *
 * To analyze response, we will compare the same string without the last character, the the last character, so no need to declare 3 strings
 *
 * char teemaster_list_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's', 't' };
 * char teemaster_lis2_header[14] = { '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', 'l', 'i', 's', '2' };
 */

int len_ipv4_header = 12;
char ipv4_header[12] = { '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\xFF', '\xFF' };

query_status_t
send_teeserver_request_packet(struct qserver *server)
{
	query_status_t ret;

	/*
	 * Try first then second then third...
	 * In fact the master server said which server use which protocol, but how qstat can transmit this information?
	 */

	/* Send v1 packet */
	teeserver_request_packet[13] = 'f';
	ret = send_packet(server, teeserver_request_packet, 13);
	if (ret != INPROGRESS) {
		return (ret);
	}

	/* Send v2 packet */
	teeserver_request_packet[13] = '2';
	ret = send_packet(server, teeserver_request_packet, len_teeserver_request_packet);
	if (ret != INPROGRESS) {
		return (ret);
	}

	/* Send v2 packet */
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

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	/* packet too short */
	if (len_teeserver_info_headerprefix > rawpktlen) {
		return (PKT_ERROR);
	}

	/* not null-terminated packet */
	if ((strnlen(rawpkt, rawpktlen) == rawpktlen) && (rawpkt[rawpktlen] != 0)) {
		return (PKT_ERROR);
	}

	/* get the last character */
	last_char = rawpkt[len_teeserver_info_headerprefix];

	/* compare the response without the last character, and verify if the last character is 'o', '2' or '3' */
	if ((memcmp(rawpkt, teeserver_info_headerprefix, len_teeserver_info_headerprefix) != 0) || ((last_char != 'o') && (last_char != '2') && (last_char != '3'))) {
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
		/* Is there a difference between a teeworld spectator and what qstat calls a "client"?  */

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
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x000F) << 12;

	tok = strtok(NULL, ".");
	if (tok == NULL) {
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x000F) << 8;

	tok = strtok(NULL, ".");
	if (tok == NULL) {
		return (PKT_ERROR);
	}
	server->protocol_version |= (atoi(tok) & 0x00FF);

	return (DONE_FORCE);
}


query_status_t
send_teemaster_request_packet(struct qserver *server)
{
	query_status_t ret_packet, ret_packe2;

	teemaster_packet[13] = 't';
	ret_packet = send_packet(server, teemaster_packet, len_teemaster_packet);

	if (ret_packet != INPROGRESS) {
		return (ret_packet);
	}

	teemaster_packet[13] = '2';
	ret_packe2 = send_packet(server, teemaster_packet, len_teemaster_packet);

	return (ret_packe2);
}


query_status_t
deal_with_teemaster_packet(struct qserver *server, char *rawpkt, int rawpktlen)
{
	int i;
	char *current;
	int previous_len, num_servers;
	int len_address_packet;
	char last_char;

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	if (len_teemaster_list_headerprefix > rawpktlen) {
		return (PKT_ERROR);
	}

	/* get the last character */
	last_char = rawpkt[len_teemaster_list_headerprefix];

	/* compare the response without the last character */
	if ((memcmp(rawpkt, teemaster_list_headerprefix, len_teemaster_list_headerprefix)) != 0) {
		return (PKT_ERROR);
	}

	/* legacy server list format, only ipv4 addresses */
	if (last_char == 't') {
		len_address_packet = 6;
		/* normal server list format, ipv4 and ipv6 addresses */
	} else if (last_char == '2') {
		len_address_packet = 18;
		/* bad or unknown server list format */
	} else {
		return (PKT_ERROR);
	}

	previous_len = server->master_pkt_len;
	num_servers = (rawpktlen - len_teemaster_list_header) / len_address_packet;
	server->n_servers += num_servers;
	server->master_pkt_len += num_servers * 6;
	server->master_pkt = (char *)realloc(server->master_pkt, server->master_pkt_len);
	current = server->master_pkt + previous_len;

	rawpkt += len_teemaster_list_header;
	rawpktlen -= len_teemaster_list_header;
	for (i = 0; i < num_servers; i++) {
		if (len_address_packet > rawpktlen) {
			return (PKT_ERROR);
		}

		/* ipv4-only server list */
		if (len_address_packet == 6) {
			memcpy(current, rawpkt, 6);
			memcpy(current + 5, rawpkt + 4, 1);
			memcpy(current + 4, rawpkt + 5, 1);
			/* ipv4 and ipv6 server list */
		} else {
			/* ipv4 server address */
			if (memcmp(rawpkt, ipv4_header, len_ipv4_header) == 0) {
				memcpy(current, rawpkt + len_ipv4_header, 6);
			}
			/* else it's an ipv6 server address, which is currently unsuported, hence ignored */
		}

		current += 6;
		rawpkt += len_address_packet;
		rawpktlen -= len_address_packet;
	}

	if (len_address_packet == 6) {
		return (INPROGRESS);
	}

	server->server_name = MASTER;
	return (DONE_AUTO);
}
