/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * New Half-Life2 query protocol
 * Copyright 2005 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"

#define A2S_GETCHALLENGE	  "\xFF\xFF\xFF\xFF\x57"
#define A2S_CHALLENGERESPONSE 0x41
#define A2S_INFO			  "\xFF\xFF\xFF\xFF\x54Source Engine Query"
#define A2S_INFORESPONSE_HL1  0x6D
#define A2S_INFORESPONSE_HL2  0x49
#define A2S_PLAYER			"\xFF\xFF\xFF\xFF\x55"
#define A2S_PLAYERRESPONSE	0x44
#define A2S_RULES			 "\xFF\xFF\xFF\xFF\x56"
#define A2S_RULESRESPONSE	 0x45

struct a2s_status
{
	unsigned sent_challenge : 1;
	unsigned have_challenge : 1;
	unsigned sent_info : 1;
	unsigned have_info : 1;
	unsigned sent_player : 1;
	unsigned have_player : 1;
	unsigned sent_rules : 1;
	unsigned have_rules : 1;
	unsigned challenge;
	unsigned char type;
};

void send_a2s_request_packet(struct qserver *server)
{
	struct a2s_status* status = (struct a2s_status*)server->master_query_tag;

	if(qserver_send_initial(server, A2S_INFO, sizeof(A2S_INFO)) == -1)
		goto error;

	status->sent_info = 1;
	status->type = 0;

	if(get_server_rules || get_player_info)
		server->next_rule = ""; // trigger calling send_a2s_rule_request_packet

	return;

error:
	cleanup_qserver(server, 1);
}

void send_a2s_rule_request_packet(struct qserver *server)
{
	struct a2s_status* status = (struct a2s_status*)server->master_query_tag;

	if(!get_server_rules && !get_player_info)
	{
		goto error;
	}

	do
	{
		if(!status->have_challenge)
		{
			debug(3, "sending challenge");
			if(qserver_send_initial(server, A2S_GETCHALLENGE, sizeof(A2S_GETCHALLENGE)-1) == -1)
				goto error;
			status->sent_challenge = 1;
			break;
		}
		else if(get_server_rules && !status->have_rules)
		{
			char buf[sizeof(A2S_RULES)-1+4] = A2S_RULES;
			memcpy(buf+sizeof(A2S_RULES)-1, &status->challenge, 4);
			debug(3, "sending rule query");
			if(qserver_send_initial(server, buf, sizeof(buf)) == -1)
				goto error;
			status->sent_rules = 1;
			break;
		}
		else if(get_player_info && !status->have_player)
		{
			char buf[sizeof(A2S_PLAYER)-1+4] = A2S_PLAYER;
			memcpy(buf+sizeof(A2S_PLAYER)-1, &status->challenge, 4);
			debug(3, "sending player query");
			if(qserver_send_initial(server, buf, sizeof(buf)) == -1)
				goto error;
			status->sent_player = 1;
			break;
		}
		else
		{
			debug(3, "timeout");
			// we are probably called due to timeout, restart.
			status->have_challenge = 0;
			status->have_rules = 0;
		}
	} while(1);

	return;

error:
	cleanup_qserver(server, 1);
}

void deal_with_a2s_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	struct a2s_status* status = (struct a2s_status*)server->master_query_tag;
	unsigned char* pkt = rawpkt;
	char buf[16];
	unsigned char* str;
	unsigned cnt;

	if(server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1);
		server->n_requests++;
	}

	if(pktlen < 5) goto out_too_short;

	if( 0 == memcmp(pkt, "\xFE\xFF\xFF\xFF", 4) )
	{
		// fragmented packet
		unsigned char pkt_index, pkt_max;
		unsigned int pkt_id = 1;
		SavedData *sdata;

		if(pktlen < 9) goto out_too_short;

		pkt += 4;

		// format:
		// int sequenceNumber
		// byte packetId
		// packetId format:
		// bits 0 - 3 = packets position in the sequence ( 0 .. N - 1 )
		// bits 4 - 7 = total number of packets

		// sequenceId
		memcpy( &pkt_id, pkt, 4 );
		debug( 3, "sequenceId: %d", pkt_id );
		pkt += 4;

		// packetId
		if ( 1 == status->type )
		{
			// HL2 format
			// The lower four bits represent the number of packets (2 to 15) and
			// the upper four bits represent the current packet starting with 0
			pkt_max = ((unsigned char)*pkt) & 15;
			pkt_index = ((unsigned char)*pkt) >> 4;
			debug( 3, "packetid: 0x%hhx => idx: %hhu, max: %hhu", *pkt, pkt_index, pkt_max );
			pkt++;
			pktlen -= 9;
		}
		else if ( 2 == status->type )
		{
			// HL2 format
			// The next two bytes are:
			// 1. the max packets sent
			// 2. the index of this packet starting from 0
			if(pktlen < 10) goto out_too_short;
			pkt_max = ((unsigned char)*pkt);
			pkt_index = ((unsigned char)*(pkt+1));
			debug( 3, "packetid: 0x%hhx => idx: %hhu, max: %hhu", *pkt, pkt_index, pkt_max );
			pkt+=2;
			pktlen -= 10;
		}
		else
		{
			malformed_packet( server, "Unable to determine packet format" );
			cleanup_qserver( server, 1 );
			return;
		}

		// pkt_max is the total number of packets expected
		// pkt_index is a bit mask of the packets received.

		if ( server->saved_data.data == NULL )
		{
			sdata = &server->saved_data;
		}
		else
		{
			sdata = (SavedData*) calloc( 1, sizeof(SavedData));
			sdata->next = server->saved_data.next;
			server->saved_data.next = sdata;
		}

		sdata->pkt_index = pkt_index;
		sdata->pkt_max = pkt_max;
		sdata->pkt_id = pkt_id;
		sdata->datalen = pktlen;
		sdata->data= (char*) malloc( sdata->datalen);
		if ( NULL == sdata->data )
		{
			malformed_packet(server, "Out of memory");
			cleanup_qserver( server, 1 );
			return;
		}
		memcpy( sdata->data, pkt, sdata->datalen);

		// combine_packets will call us recursively
		combine_packets( server );
		return;
	}
	else if( 0 != memcmp(pkt, "\xFF\xFF\xFF\xFF", 4) )
	{
		malformed_packet(server, "invalid packet header");
		goto out_error;
	}

	pkt += 4;
	pktlen -= 4;

	pktlen -= 1;
	switch(*pkt++)
	{
	case A2S_CHALLENGERESPONSE:
		if(pktlen < 4) goto out_too_short;
		memcpy(&status->challenge, pkt, 4);
		// do not count challenge as retry
		if(!status->have_challenge && server->retry1 != n_retries)
		{
			++server->retry1;
			if(server->n_retries)
			{
				--server->n_retries;
			}
		}
		status->have_challenge = 1;
		debug(3, "challenge %x", status->challenge);
		break;

	case A2S_INFORESPONSE_HL1:
		if(pktlen < 28) goto out_too_short;
		status->type = 1;

		// ip:port
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		//server->server_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// server name
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->server_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// map
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->map_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// mod dir
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->game = strdup(pkt);
		add_rule(server, "gamedir", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// mod description
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "gamename", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		if( pktlen < 7 ) goto out_too_short;

		// num players
		server->num_players = pkt[0];

		// max players
		server->max_players = pkt[1];

		// version
		sprintf( buf, "%hhu", pkt[2] );
		add_rule( server, "version", buf, 0 );

		// dedicated
		add_rule( server, "dedicated", pkt[3] == 'd' ? "1" : "0", 0 );

		// os
		switch( pkt[4] )
		{
		case 'l':
			add_rule(server, "sv_os", "linux", 0);
			break;

		case 'w':
			add_rule(server, "sv_os", "windows", 0);
			break;

		default:
			buf[0] = pkt[4];
			buf[1] = '\0';
			add_rule(server, "sv_os", buf, 0);
		}

		// password
		add_rule(server, "password", pkt[5] ? "1" : "0" , 0);

		pkt += 6;
		pktlen -= 6;

		// mod info
		if ( pkt[0] )
		{
			pkt++;
			pktlen--;

			// mod URL
			str = memchr(pkt, '\0', pktlen);
			if(!str) goto out_too_short;
			add_rule(server, "mod_url", strdup( pkt ), 0);
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			// mod DL
			str = memchr(pkt, '\0', pktlen);
			if(!str) goto out_too_short;
			add_rule(server, "mod_dl", strdup( pkt ), 0);
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			// mod Empty
			str = memchr(pkt, '\0', pktlen);
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			if( pktlen < 10 ) goto out_too_short;

			// mod version
			sprintf( buf, "%u", swap_long_from_little(pkt));
			add_rule( server, "mod_ver", buf, 0 );
			pkt += 4;
			pktlen -= 4;

			// mod size
			sprintf( buf, "%u", swap_long_from_little(pkt));
			add_rule( server, "mod_size", buf, 0 );
			pkt += 4;
			pktlen -= 4;

			// svonly
			add_rule( server, "mod_svonly", ( *pkt ) ? "1" : "0" , 0 );
			pkt++;
			pktlen--;

			// cldll
			add_rule( server, "mod_cldll", ( *pkt ) ? "1" : "0" , 0 );
			pkt++;
			pktlen--;
		}

		if( pktlen < 2 ) goto out_too_short;

		// Secure
		add_rule( server, "secure", *pkt ? "1" : "0" , 0 );
		pkt++;
		pktlen--;

		// Bots
		sprintf( buf, "%hhu", *pkt );
		add_rule( server, "bots", buf, 0 );
		pkt++;
		pktlen--;

		status->have_info = 1;

		server->retry1 = n_retries;

		server->next_player_info = server->num_players;

		break;

	case A2S_INFORESPONSE_HL2:
		if(pktlen < 1) goto out_too_short;
		status->type = 2;
		snprintf(buf, sizeof(buf), "%hhX", *pkt);
		add_rule(server, "protocol", buf, 0);
		++pkt; --pktlen;

		// server name
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->server_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// map
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->map_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// mod
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->game = strdup(pkt);
		add_rule(server, "gamedir", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// description
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "gamename", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		if(pktlen < 9) goto out_too_short;

		// pkt[0], pkt[1] steam id unused
		server->num_players = pkt[2];
		server->max_players = pkt[3];
		// pkt[4] number of bots
		add_rule(server, "dedicated", pkt[5]?"1":"0", 0);
		if(pkt[6] == 'l')
		{
			add_rule(server, "sv_os", "linux", 0);
		}
		else if(pkt[6] == 'w')
		{
			add_rule(server, "sv_os", "windows", 0);
		}
		else
		{
			buf[0] = pkt[6];
			buf[1] = '\0';
			add_rule(server, "sv_os", buf, 0);
		}

		if(pkt[7])
		{
			snprintf(buf, sizeof(buf), "%u", (unsigned)pkt[7]);
			add_rule(server, "password", buf, 0);
		}

		if(pkt[8])
		{
			snprintf(buf, sizeof(buf), "%u", (unsigned)pkt[8]);
			add_rule(server, "secure", buf, 0);
		}

		pkt += 9;
		pktlen -= 9;

		// version
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "version", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		status->have_info = 1;

		server->retry1 = n_retries;

		server->next_player_info = server->num_players;

		break;

	case A2S_RULESRESPONSE:

		if(pktlen < 2) goto out_too_short;

		cnt = pkt[0] + (pkt[1]<<8);
		pktlen -= 2;
		pkt += 2;

		debug(3, "num_rules: %d", cnt);

		for(;cnt && pktlen > 0; --cnt)
		{
			char* key, *value;
			str = memchr(pkt, '\0', pktlen);
			if(!str) break;
			key = pkt;
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			str = memchr(pkt, '\0', pktlen);
			if(!str) break;
			value = pkt;
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			add_rule(server, key, value, NO_FLAGS);
		}

		if(cnt)
		{
			malformed_packet(server, "packet contains too few rules, missing %d", cnt);
			server->missing_rules = 1;
		}
		if(pktlen)
		malformed_packet(server, "garbage at end of rules, %d bytes left", pktlen);

		status->have_rules = 1;

		server->retry1 = n_retries;

		break;

	case A2S_PLAYERRESPONSE:

		if(pktlen < 1) goto out_too_short;

		cnt = pkt[0];
		pktlen -= 1;
		pkt += 1;

		debug(3, "num_players: %d", cnt);

		for(;cnt && pktlen > 0; --cnt)
		{
			unsigned idx;
			const char* name;
			struct player* p;

			idx = *pkt++;
			--pktlen;

			str = memchr(pkt, '\0', pktlen);
			if(!str) break;
			name = pkt;
			pktlen -= str-pkt+1;
			pkt += str-pkt+1;

			if(pktlen < 8) goto out_too_short;

			debug(3, "player index %d", idx);
			p = add_player(server, server->n_player_info);
			if(p)
			{
				p->name = strdup(name);
				p->frags = swap_long_from_little(pkt);
				p->connect_time = swap_float_from_little(pkt+4);
			}
			pktlen -= 8;
			pkt += 8;
		}

#if 0 // seems to be a rather normal condition
		if(cnt)
		{
			malformed_packet(server, "packet contains too few players, missing %d", cnt);
		}
#endif
		if(pktlen)
		malformed_packet(server, "garbage at end of player info, %d bytes left", pktlen);

		status->have_player = 1;

		server->retry1 = n_retries;

		break;

	default:
		malformed_packet(server, "invalid packet id %hhx", *--pkt);
		goto out_error;
	}

	if(
		(!get_player_info || (get_player_info && status->have_player)) &&
		(!get_server_rules || (get_server_rules && status->have_rules))
	)
	{
		server->next_rule = NULL;
	}

	cleanup_qserver(server, 0);
	return;

out_too_short:
	malformed_packet(server, "packet too short");

out_error:
	cleanup_qserver(server, 1);
}

// vim: sw=4 ts=8 noet
