/*
 * qstat 2.11
 * by Steve Jankowski
 *
 * Teeworlds protocol
 * Copyright 2008 ? Emiliano Leporati
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

char tee_serverinfo[8] = { '\xFF', '\xFF', '\xFF', '\xFF', 'i', 'n', 'f', 'o' };

int send_tee_request_packet( struct qserver *server )
{
// printf("send packet %d\n", server->n_requests);
	return send_packet( server, server->type->status_packet, server->type->status_len );
}


int deal_with_tee_packet( struct qserver *server, char *rawpkt, int pktlen )
{
// printf("receive packet %d len %d\n", server->n_requests, pktlen);
	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	// skip unimplemented ack, crc, etc
	char *pkt = rawpkt + 6;
	char *tok = NULL, *version = NULL;
	int i;
	struct player* player;
	if (0 == memcmp( pkt, tee_serverinfo, 8)) 
	{
// printf("info ok\n");
		pkt += 8;
		// version
		version = strdup(pkt); pkt += strlen(pkt) + 1;
		// server name
		server->server_name = strdup(pkt); pkt += strlen(pkt) + 1;
		// map name
		server->map_name = strdup(pkt); pkt += strlen(pkt) + 1;
		// game type
		switch(atoi(pkt)) {
		case 0:
			add_rule( server, server->type->game_rule, "dm", NO_FLAGS);
			break;
		case 1:
			add_rule( server, server->type->game_rule, "tdm", NO_FLAGS);
			break;
		case 2:
			add_rule( server, server->type->game_rule, "ctf", NO_FLAGS);
			break;
		default:
			add_rule( server, server->type->game_rule, "unknown", NO_FLAGS);
			break;
		}
		pkt += strlen(pkt) + 1; 
		pkt += strlen(pkt) + 1;
		pkt += strlen(pkt) + 1;
		// num players
		server->num_players = atoi(pkt); pkt += strlen(pkt) + 1;
// printf("num players %d\n", server->num_players);
		// max players
		server->max_players = atoi(pkt); pkt += strlen(pkt) + 1;
		// players
// printf("max players %d\n", server->max_players);
		for(i = 0; i < server->num_players; i++) {
			player = add_player( server, i );
// printf("player %s ", pkt);
			player->name = strdup(pkt); pkt += strlen(pkt) + 1;
// printf("%s\n", pkt);
			player->score = atoi(pkt); pkt += strlen(pkt) + 1;
// fflush(stdout);
		}
// printf("players ok");		
// fflush(stdout);
		// version reprise
		server->protocol_version = 0;

		if (NULL == (tok = strtok(version, "."))) return -1;
		server->protocol_version |= (atoi(tok) & 0x000F) << 12;
		if (NULL == (tok = strtok(NULL, "."))) return -1;
		server->protocol_version |= (atoi(tok) & 0x000F) << 8;
		if (NULL == (tok = strtok(NULL, "."))) return -1;
		server->protocol_version |= (atoi(tok) & 0x00FF);

		free(version);

		return DONE_FORCE;
	}

	// unknown packet type
	return -1;
}
