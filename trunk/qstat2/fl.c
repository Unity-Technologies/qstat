/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Frontlines-Fuel of War protocol
 * Copyright 2008 Steven Hartland
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

#define FL_GETCHALLENGE	  "\xFF\xFF\xFF\xFF\x57"
#define FL_CHALLENGERESPONSE 0x41
#define FL_INFO			  "\xFF\xFF\xFF\xFF\x46LSQ"
#define FL_INFORESPONSE  0x49
#define FL_PLAYER			"\xFF\xFF\xFF\xFF\x55"
#define FL_PLAYERRESPONSE	0x44
#define FL_RULES			 "\xFF\xFF\xFF\xFF\x56"
#define FL_RULESRESPONSE	 0x45

struct fl_status
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

void send_fl_request_packet(struct qserver *server)
{
	struct fl_status* status = (struct fl_status*)server->master_query_tag;

	if( -1 == qserver_send_initial(server, FL_INFO, sizeof(FL_INFO)) )
	{
		cleanup_qserver(server, 1);
		return;
	}

	status->sent_info = 1;
	status->type = 0;

	/*
	TODO: uncomment when its supported
	if(get_server_rules || get_player_info)
	{
		server->next_rule = ""; // trigger calling send_fl_rule_request_packet
	}
	*/
}

void send_fl_rule_request_packet(struct qserver *server)
{
	struct fl_status* status = (struct fl_status*)server->master_query_tag;

	// TODO: remove when its supported
	goto error;

	if(!get_server_rules && !get_player_info)
	{
		goto error;
	}

	do
	{
		if(!status->have_challenge)
		{
			debug(3, "sending challenge");
			if(qserver_send_initial(server, FL_GETCHALLENGE, sizeof(FL_GETCHALLENGE)-1) == -1)
				goto error;
			status->sent_challenge = 1;
			break;
		}
		else if(get_server_rules && !status->have_rules)
		{
			char buf[sizeof(FL_RULES)-1+4] = FL_RULES;
			memcpy(buf+sizeof(FL_RULES)-1, &status->challenge, 4);
			debug(3, "sending rule query");
			if(qserver_send_initial(server, buf, sizeof(buf)) == -1)
				goto error;
			status->sent_rules = 1;
			break;
		}
		else if(get_player_info && !status->have_player)
		{
			char buf[sizeof(FL_PLAYER)-1+4] = FL_PLAYER;
			memcpy(buf+sizeof(FL_PLAYER)-1, &status->challenge, 4);
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

void deal_with_fl_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	struct fl_status* status = (struct fl_status*)server->master_query_tag;
	char* pkt = rawpkt;
	char buf[16];
	char* str;
	unsigned cnt;
	unsigned short tmp_short;

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
			// FL format
			// The next two bytes are:
			// 1. the max packets sent ( byte )
			// 2. the index of this packet starting from 0 ( byte )
			// 3. Size of the split ( short )
			if(pktlen < 10) goto out_too_short;
			pkt_max = ((unsigned char)*pkt);
			pkt_index = ((unsigned char)*(pkt+1));
			debug( 3, "packetid[2]: 0x%hhx => idx: %hhu, max: %hhu", *pkt, pkt_index, pkt_max );
			pkt+=4;
			pktlen -= 12;
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
	debug( 2, "FL type = %hhd", *pkt );
	switch(*pkt++)
	{
	case FL_CHALLENGERESPONSE:
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

	case FL_INFORESPONSE:
		if(pktlen < 1) goto out_too_short;
		status->type = 1;
		snprintf(buf, sizeof(buf), "%hhX", *pkt);
		add_rule(server, "protocol", buf, 0);
		++pkt; --pktlen;

		// ServerName
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->server_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// MapName
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->map_name = strdup(pkt);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// ModName
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		server->game = strdup(pkt);
		add_rule(server, "modname", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// GameMode
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "gamemode", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// GameDescription
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "gamedescription", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		// GameVersion
		str = memchr(pkt, '\0', pktlen);
		if(!str) goto out_too_short;
		add_rule(server, "gameversion", pkt, 0);
		pktlen -= str-pkt+1;
		pkt += str-pkt+1;

		if( pktlen < 13 )
		{
			goto out_too_short;
		}

		// GamePort
		tmp_short = ((unsigned short)pkt[0] <<8 ) | ((unsigned short)pkt[1]);
		change_server_port( server, tmp_short, 0 );
		pkt += 2;

		// Num Players
		server->num_players = (unsigned char)*pkt++;

		// Max Players
		server->max_players = (unsigned char)*pkt++;

		// Dedicated
		add_rule(server, "dedicated", ( 'd' == *pkt++) ? "1" : "0", 0);

		// OS
		switch( *pkt )
		{
		case 'l':
			add_rule(server, "sv_os", "linux", 0);
			break;

		case 'w':
			add_rule(server, "sv_os", "windows", 0);
			break;

		default:
			buf[0] = *pkt;
			buf[1] = '\0';
			add_rule(server, "sv_os", buf, 0);
			break;
		}
		pkt++;

		// Passworded
		add_rule(server, "passworded", ( *pkt++ ) ? "1" : "0" , 0);

		// Anticheat
		add_rule(server, "passworded", ( *pkt++ ) ? "1" : "0" , 0);

		// FrameTime
		sprintf( buf, "%hhu", *pkt++ );
		add_rule(server, "frametime", buf , 0);

		// Round
		sprintf( buf, "%hhu", *pkt++ );
		add_rule(server, "round", buf , 0);

		// RoundMax
		sprintf( buf, "%hhu", *pkt++ );
		add_rule(server, "roundmax", buf , 0);

		// RoundSeconds
		tmp_short = ((unsigned short)pkt[0] <<8 ) | ((unsigned short)pkt[1]);
		sprintf( buf, "%hhu", tmp_short );
		add_rule(server, "roundseconds", buf , 0);
		pkt += 2;

		status->have_info = 1;

		server->retry1 = n_retries;

		server->next_player_info = server->num_players;

		break;

	case FL_RULESRESPONSE:

		if(pktlen < 2) goto out_too_short;

		cnt = (unsigned char)pkt[0] + ((unsigned char)pkt[1]<<8);
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

	case FL_PLAYERRESPONSE:

		if(pktlen < 1) goto out_too_short;

		cnt = (unsigned char)pkt[0];
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
