/*
 * qstat 2.9
 * by Steve Jankowski
 *
 * Doom3 / Quake4 protocol
 * Copyright 2005 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "qstat.h"
#include "debug.h"
#include "assert.h"

static const char doom3_master_query[] = "\xFF\xFFgetServers\x00\x00\x00\x00\x00\x00";
//                                                  version ^^^^^^^^^^^^^^^^
//                                               null terminated mod string ^^^^
//                                                                   filterbyte ^^^^


static const char quake4_master_query[] = "\xFF\xFFgetServers\x00\x00\x00\x00\x00\x00\x00\x00";
//                                                   version ^^^^^^^^^^^^^^^^
//                                                null terminated mod string ^^^^
//                                                 null terminated player string ^^^^
//                                                       null terminated clan string ^^^^
//                                                                            filterbyte ^^^^

static unsigned put_param_string(struct qserver* server, const char* paramname, char* buf, unsigned buflen, unsigned off)
{
	char* val = get_param_value( server, paramname, NULL);
	if(val && strlen(val) < buflen-off-2)
	{
		strcpy(buf+off, val);
		off += strlen(val) + 1;
	}
	else
	{
		buf[off++] = '\0';
	}

	return off;
}

static char* build_doom3_masterfilter(struct qserver* server, char* buf, unsigned* buflen, int q4)
{
	int flen = 0;
	char *pkt, *r, *sep= "";
	unsigned char b = 0;
	char *proto = server->query_arg;
	unsigned ver;
	unsigned off = 13;

	if(!proto)
	{
		if(q4)
		{
			*buflen = sizeof(quake4_master_query);
			return (char*)quake4_master_query;
		}
		else
		{
			*buflen = sizeof(doom3_master_query);
			return (char*)doom3_master_query;
		}
	}

	ver = (atoi(proto) & 0xFFFF) << 16;
	proto = strchr(proto, '.');
	if(proto && *++proto)
	{
		ver |= (atoi(proto) & 0xFFFF);
	}
	if(q4)
	{
		ver |= 1 << 31; // third party flag
	}
	memcpy(buf, doom3_master_query, sizeof(doom3_master_query));
	put_long_little(ver, buf+off);
	off += 4;

	off = put_param_string(server, "game", buf, *buflen, off);

	if(q4)
	{
		off = put_param_string(server, "player", buf, *buflen, off);
		off = put_param_string(server, "clan", buf, *buflen, off);
	}

	pkt = get_param_value( server, "status", NULL);
	r = pkt ;
	while ( pkt && sep )
	{
		sep= strchr( r, ':');
		if ( sep )
			flen= sep-r;
		else
			flen= strlen(r);

		if ( strncmp( r, "password", flen) == 0)
			b |= 0x01;
		else if ( strncmp( r, "nopassword", flen) == 0)
			b |= 0x02;
		else if ( strncmp( r, "notfull", flen) == 0)
			b |= 0x04;
		else if ( strncmp( r, "notfullnotempty", flen) == 0)
			b |= 0x08;
		r= sep+1;
	}

	pkt = get_param_value( server, "gametype", NULL);
	if(pkt)
	{
		if ( strncmp( pkt, "dm", flen) == 0)
			b |= 0x10;
		else if ( strncmp( pkt, "tourney", flen) == 0)
			b |= 0x20;
		else if ( strncmp( pkt, "tdm", flen) == 0)
			b |= 0x30;
	}

	buf[off++] = (char)b;
	*buflen = off;

	return buf;
}

void send_doom3master_request_packet( struct qserver *server)
{
	int rc = 0;
	int packet_len = -1;
	char* packet = NULL;
	char query_buf[4096] = {0};

	server->next_player_info = NO_PLAYER_INFO;

	packet_len = sizeof(query_buf);
	packet = build_doom3_masterfilter(server, query_buf, (unsigned*)&packet_len, 0);

	rc= send( server->fd, packet, packet_len, 0);
	if ( rc == SOCKET_ERROR)
	{
		perror( "send");
	}

	if ( server->retry1 == n_retries)
	{
		gettimeofday( &server->packet_time1, NULL);
		server->n_requests++;
	}
	else
	{
		server->n_retries++;
	}
	server->retry1--;
	server->n_packets++;
}

void send_quake4master_request_packet( struct qserver *server)
{
	int rc = 0;
	int packet_len = -1;
	char* packet = NULL;
	char query_buf[4096] = {0};

	server->next_player_info = NO_PLAYER_INFO;

	packet_len = sizeof(query_buf);
	packet = build_doom3_masterfilter(server, query_buf, (unsigned*)&packet_len, 1);

	rc= send( server->fd, packet, packet_len, 0);
	if ( rc == SOCKET_ERROR)
	{
		perror( "send");
	}

	if ( server->retry1 == n_retries)
	{
		gettimeofday( &server->packet_time1, NULL);
		server->n_requests++;
	}
	else
	{
		server->n_retries++;
	}
	server->retry1--;
	server->n_packets++;
}

static const char doom3_masterresponse[] = "\xFF\xFFservers";

void
deal_with_doom3master_packet( struct qserver *server, char *rawpkt, int pktlen)
{
	char* pkt, *dest;
	int len;
	server->ping_total+= time_delta( &packet_recv_time,
		&server->packet_time1);

	if ( pktlen < sizeof(doom3_masterresponse) + 6 // at least one server
			|| (pktlen - sizeof(doom3_masterresponse)) % 6
			|| memcmp( doom3_masterresponse, rawpkt, sizeof(doom3_masterresponse) ) != 0 )
	{
		server->server_name= SERVERERROR;
		server->master_pkt_len = 0;
		malformed_packet(server, "too short or invalid response");
		cleanup_qserver( server, 1);
		return;
	}

	server->retry1 = 0; // received at least one packet so no need to retry

	pkt = rawpkt + sizeof(doom3_masterresponse);
	len = pktlen - sizeof(doom3_masterresponse);

	server->master_pkt = (char*)realloc( server->master_pkt, server->master_pkt_len + len);

	dest = server->master_pkt + server->master_pkt_len;

	server->master_pkt_len += len;

	while(len > 0)
	{
		memcpy(dest, pkt, 4 );
		dest[4] = pkt[5];
		dest[5] = pkt[4];
		dest += 6;
		pkt += 6;
		len -= 6;
	}
	assert(len == 0);

	server->n_servers= server->master_pkt_len / 6;

	debug(2, "%d servers added", server->n_servers);
}

static const char doom3_inforesponse[] = "\xFF\xFFinfoResponse";
static unsigned MAX_DOOM3_ASYNC_CLIENTS = 32;

static void _deal_with_doom3_packet( struct qserver *server, char *rawpkt, int pktlen, unsigned version )
{
	char *ptr = rawpkt;
	char *end = rawpkt + pktlen;
	int type = 0;
	int size = 0;
	unsigned num_players = 0;
	unsigned challenge = 0;
	unsigned protocolver = 0;
	char tmp[32];

	server->n_servers++;
	if ( server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1);
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	// Check if correct reply
	if ( pktlen < sizeof(doom3_inforesponse) +4 +4 +1 ||
		memcmp( doom3_inforesponse, ptr, sizeof(doom3_inforesponse)) != 0 )
	{
		malformed_packet(server, "too short or invalid response");
		cleanup_qserver( server, 1);
		return;
	}
	ptr += sizeof(doom3_inforesponse);

	challenge = swap_long_from_little(ptr);
	ptr += 4;

	if ( 5 == version )
	{
		// Something else unknown but usually FFFFFFFF
		ptr += 4;
	}

	protocolver = swap_long_from_little(ptr);
	ptr += 4;

	snprintf(tmp, sizeof(tmp), "%u.%u", protocolver >> 16, protocolver & 0xFFFF);
	debug(2, "challenge: 0x%08X, protocol: %s (0x%X)", challenge, tmp, protocolver);

	if ( 5 == version )
	{
		// Size Long
		size = swap_long_from_little(ptr);
		debug( 2, "Size = %d", size );
		ptr += 4;
	}

// Commented out until we have a better way to specify the expected version
// This is due to prey demo requiring version 4 yet prey retail version 3
/*
	if( protocolver >> 16 != version )
	{
		malformed_packet(server, "protocol version %u, expected %u", protocolver >> 16, version );
		cleanup_qserver( server, 1);
		return;
	}
*/

	server->protocol_version = protocolver;
	add_rule( server, "protocol", tmp, NO_FLAGS );

	while ( ptr < end )
	{
		// server info:
		// name value pairs null seperated
		// empty name && value signifies the end of section
		char *key, *val;
		unsigned keylen, vallen;

		key = ptr;
		ptr = memchr(ptr, '\0', end-ptr);
		if ( !ptr )
		{
			malformed_packet( server, "no rule key" );
			cleanup_qserver( server, 1);
			return;
		}
		keylen = ptr - key;

		val = ++ptr;
		ptr = memchr(ptr, '\0', end-ptr);
		if ( !ptr )
		{
			malformed_packet( server, "no rule value" );
			cleanup_qserver( server, 1);
			return;
		}
		vallen = ptr - val;
		++ptr;

		if( keylen == 0 && vallen == 0 )
		{
			type = 1;
			break; // end
		}

		debug( 2, "key:%s=%s:", key, val);

		// Lets see what we've got
		if ( 0 == strcasecmp( key, "si_name" ) )
		{
			server->server_name = strdup( val );
		}
		else if( 0 == strcasecmp( key, "fs_game" ) )
		{
			server->game = strdup( val );
		}
#if 0
		else if( 0 == strcasecmp( key, "si_version" ) )
		{
			// format:
			// DOOM 1.0.1262.win-x86 Jul  8 2004 16:46:37
			server->protocol_version = atoi( val+1 );
		}
#endif
		else if( 0 == strcasecmp( key, "si_map" ) )
		{
			server->map_name = strdup( val );
		}
		else if( 0 == strcasecmp( key, "si_maxplayers" ) )
		{
			server->max_players = atoi( val );
		}

		add_rule( server, key, val, NO_FLAGS );
	}

	if ( type != 1 )
	{
		// no more info should be player headers here as we
		// requested it
		malformed_packet( server, "player info missing" );
		cleanup_qserver( server, 1);
		return;
	}

	// now each player details
	while( ptr < end )
	{
		struct player *player;
		char *val;
		unsigned char player_id = *ptr++;
		short prediction = 0;
		unsigned rate = 0;

		if(player_id == MAX_DOOM3_ASYNC_CLIENTS)
			break;
debug( 2, "ID = %d\n", player_id );
		if ( end - ptr == 4 )
		{
			// Just an osmask left
			break;
		}

		/* id's are not steady
		if(player_id != num_players)
		{
			malformed_packet(server, "unexpected player id");
			cleanup_qserver( server, 1);
			return;
		}
		*/

		if ( ptr + 7 > end ) // 2 pred + 4 rate + empty player name ('\0')
		{
			// run off the end and shouldnt have
			malformed_packet( server, "player info too short" );
			cleanup_qserver( server, 1);
			return;
		}

		player = add_player( server, player_id );

		player->score = 0;
		player->frags = 0;

		prediction = swap_short_from_little(ptr);
		ptr += 2;

		player->ping = prediction; // seems to be ping

		rate = swap_long_from_little(ptr);
		ptr += 4;

		{
			char buf[16];
			snprintf(buf, sizeof(buf), "%u", rate);
			player_add_info(player, "rate", buf, NO_FLAGS);
		}

		val = ptr;
		ptr = memchr(ptr, '\0', end-ptr);
		if ( !ptr )
		{
			malformed_packet( server, "player name not null terminated" );
			cleanup_qserver( server, 1);
			return;
		}
		++ptr;
		player->name = strdup( val );

		if( 2 == version )
		{
			val = ptr;
			ptr = memchr(ptr, '\0', end-ptr);
			if ( !ptr )
			{
				malformed_packet( server, "player clan not null terminated" );
				cleanup_qserver( server, 1);
				return;
			}
			++ptr;
			player->tribe_tag = strdup( val );
			debug( 2, "Player[%d] = %s, prediction %hu, rate %u, id %hhu, clan %s",
					num_players, player->name, prediction, rate, player_id, player->tribe_tag);
		}
		else
		{
			debug( 2, "Player[%d] = %s, prediction %hu, rate %u, id %hhu",
					num_players, player->name, prediction, rate, player_id );
		}

		++num_players;
	}

	if(end - ptr >= 4)
	{
		snprintf(tmp, sizeof(tmp), "0x%X", swap_long_from_little(ptr));
		add_rule( server, "osmask", tmp, NO_FLAGS );
		debug( 2, "osmask %s", tmp);
		ptr += 4;
	}
	else
	{
		malformed_packet(server, "osmask missing");
	}

#if 0
	if(end - ptr)
	{
		malformed_packet(server, "%ld byes left", end-ptr);
	}
#endif

	debug( 2, "Num players = %d", num_players );
	server->num_players = num_players;

	cleanup_qserver( server, 1 );
	return;
}

void deal_with_doom3_packet( struct qserver *server, char *rawpkt, int pktlen)
{
	_deal_with_doom3_packet( server, rawpkt, pktlen, 1 );
}

void deal_with_quake4_packet( struct qserver *server, char *rawpkt, int pktlen)
{
	_deal_with_doom3_packet( server, rawpkt, pktlen, 2 );
}

void deal_with_prey_demo_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	_deal_with_doom3_packet( server, rawpkt, pktlen, 4 );
}

void deal_with_prey_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	_deal_with_doom3_packet( server, rawpkt, pktlen, 3 );
}

void deal_with_etqw_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	_deal_with_doom3_packet( server, rawpkt, pktlen, 5 );
}
