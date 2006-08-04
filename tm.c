/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Gamespy query protocol
 * Copyright 2005 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"


void send_tm_request_packet( struct qserver *server )
{
	char buf[2048];
	char *xmlp = buf + 8;
	unsigned int len;

	if ( ! server->protocol_version )
	{
		// No seen the version yet wait
		return;
	}

	if ( get_player_info )
	{
		server->flags |= TF_PLAYER_QUERY|TF_RULES_QUERY;
		// TODO: add more calls to get full player info?
		strcpy( xmlp, "<?xml version=\"1.0\"?>\n<methodCall>\n<methodName>system.multicall</methodName>\n<params><param><value><array><data>\n<value><struct><member><name>methodName</name><value>GetServerOptions</value></member><member><name>params</name><value><array><data></data></array></value></member></struct></value>\n<value><struct><member><name>methodName</name><value>GetCurrentChallengeInfo</value></member><member><name>params</name><value><array><data></data></array></value></member></struct></value>\n<value><struct><member><name>methodName</name><value>GetPlayerList</value></member><member><name>params</name><value><array><data><value><i4>100</i4></value><value><i4>0</i4></value></data></array></value></member></struct></value>\n</data></array></value></param></params>\n</methodCall>" );
	}
	else
	{
		server->flags |= TF_STATUS_QUERY;
		strcpy( xmlp, "<?xml version=\"1.0\"?>\n<methodCall>\n<methodName>system.multicall</methodName>\n<params><param><value><array><data>\n<value><struct><member><name>methodName</name><value>GetServerOptions</value></member><member><name>params</name><value><array><data></data></array></value></member></struct></value>\n<value><struct><member><name>methodName</name><value>GetCurrentChallengeInfo</value></member><member><name>params</name><value><array><data></data></array></value></member></struct></value>\n<value><struct><member><name>methodName</name><value>GetPlayerList</value></member><member><name>params</name><value><array><data><value><i4>100</i4></value><value><i4>0</i4></value></data></array></value></member></struct></value>\n</data></array></value></param></params>\n</methodCall>" );
	}

	// First 4 bytes is the length of the request
	len = strlen( xmlp );
	memcpy( buf, &len, 4 );
	// Second 4 bytes is the handle identifier ( id )
	memcpy( buf+4, &server->challenge, 4 );

	// prep the details we need for multi packet responses
	server->saved_data.pkt_id = 0; // used to indicate what method we are processing
	server->saved_data.pkt_max = 1; // we expect at least 1 packet response

	send_packet( server, buf, len + 8 );
}


void deal_with_tm_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *end;
	char *key = NULL, *value = NULL, *tmpp = NULL;
	char fullname[256];
	struct player *player = NULL;
	int pkt_max = server->saved_data.pkt_max;
	int not_last = 1;
	debug( 2, "processing..." );

	s = rawpkt;
	if ( 4 == pktlen && 0 == memcmp( rawpkt, "\x0b\x00\x00\x00", 4 ) )
	{
		// setup handle identifier
		// greater 2^31 = XML-RPC, less = callback
		server->challenge = 0x80000001;
		return;
	}
	else if ( 10 < pktlen && 1 == sscanf( rawpkt, "GBXRemote %d", &server->protocol_version ) )
	{
		// Got protocol version send request
		send_tm_request_packet( server );
		return;	
	}
	else if ( 8 <= pktlen && 0 == memcmp( rawpkt+4, &server->challenge, 4 ) )
	{
		// first 4 bytes = the length
		// second 4 bytes = handle identifier we sent in the request
		server->saved_data.pkt_id++;
		if ( 8 == pktlen )
		{
			// split packet
			// we have at least one more packet coming
			if ( ! add_packet( server, 0, 0, 2, pktlen, rawpkt, 1 ) )
			{
				// fatal error e.g. out of memory
				return;
			}
			return;
		}
		else
		{
			s += 8;
		}
	}

	not_last = strncmp( rawpkt + pktlen - 19, "</methodResponse>", 17 );
	if ( 0 != strncmp( s, "<?xml", 5 ) || 0 != not_last )
	{
		// we dont have a complete response yet combine
		int new_max = ( not_last ) ? pkt_max + 1 : pkt_max;
		if ( ! add_packet( server, 0, pkt_max - 1, new_max, pktlen, rawpkt, 1 ) )
		{
			// fatal error e.g. out of memory
			return;
		}

		if ( ! not_last )
		{
			// we are the last packet run combine to call us back
			combine_packets( server );
		}
		return;
	}

	server->n_servers++;
	if ( server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	rawpkt[pktlen]= '\0';
	end = &rawpkt[pktlen];

//fprintf( stderr, "S=%s\n", s );
	s = strtok( s, "\015\012" );
	while ( NULL != s )
	{
//fprintf( stderr, "S=%s\n", s );
		if ( 0 == strncmp( s, "<member><name>", 14 ) )
		{
			key = s + 14;
			tmpp = strstr( key, "</name>" );
			if ( NULL != tmpp )
			{
				*tmpp = '\0';
			}
			s = strtok( NULL, "\015\012" );
			value = NULL;
			continue;
		}
		else if ( 0 == strncmp( s, "<value>", 7 ) )
		{
			// value
			s += 7;
			if ( 0 == strncmp( s, "<string>", 8 ) )
			{
				// String
				value = s+8;
				tmpp = strstr( s, "</string>" );
			}
			else if ( 0 == strncmp( s, "<i4>", 4 ) )
			{
				// Int
				value = s+4;
				tmpp = strstr( s, "</i4>" );
			}
			else if ( 0 == strncmp( s, "<boolean>", 9 ) )
			{
				// Boolean
				value = s+9;
				tmpp = strstr( s, "</boolean>" );
			}
			else if ( 0 == strncmp( s, "<double>", 8 ) )
			{
				// Double
				value = s+8;
				tmpp = strstr( s, "</double>" );
			}
			// also have struct and array but not interested in those

			if ( NULL != tmpp )
			{
				*tmpp = '\0';
			}

			if ( NULL != value )
			{
				debug( 4, "%s = %s\n", key, value );
			}
		}
		else if ( 0 == strncmp( s, "</struct>", 9 ) && 3 > server->saved_data.pkt_id )
		{
			// end of method response
			server->saved_data.pkt_id++;
		}

		if ( NULL != value )
		{
			switch( server->saved_data.pkt_id )
			{
			case 1:
				// GetServerOptions response
				if ( 0 == strcmp( "Name", key ) )
				{
					server->server_name = strdup( value );
				}
				else if ( 0 == strcmp( "CurrentMaxPlayers", key ) )
				{
					server->max_players = atoi( value );
				}
				else
				{
					sprintf( fullname, "server.%s", key );
					add_rule( server, fullname, value, NO_FLAGS);
				}
				break;
			case 2:
				// GetCurrentChallengeInfo response
				if ( 0 == strcmp( "Name", key ) )
				{
					server->map_name = strdup( value );
				}
				else
				{
					sprintf( fullname, "challenge.%s", key );
					add_rule( server, fullname, value, NO_FLAGS);
				}
				break;
			case 3:
				// GetPlayerList response
				// Player info
				if ( 0 == strcmp( "Login", key ) )
				{
					player = add_player( server, server->n_player_info );
				}
				else if ( NULL != player )
				{
					if ( 0 == strcmp( "NickName", key ) )
					{
						player->name = strdup( value );
					}
					else if ( 0 == strcmp( "PlayerId", key ) )
					{
						//player->number = atoi( value );
					}
					else if ( 0 == strcmp( "TeamId", key ) )
					{
						player->team = atoi( value );
					}
					else if ( 0 == strcmp( "IsSpectator", key ) )
					{
						player->flags = player->flags & 1;
					}
					else if ( 0 == strcmp( "IsInOfficialMode", key ) )
					{
						player->flags = player->flags & 2;
					}
					else if ( 0 == strcmp( "LadderRanking", key ) )
					{
						player->score = atoi( value );
					}
				}
				break;
			}
			value = NULL;
		}
		s = strtok( NULL, "\015\012" );
	}

	if ( 0 == strncmp( rawpkt + pktlen - 19, "</methodResponse>", 17 ) )
	{
		// last packet seen
		cleanup_qserver( server, 1 );
	}
}
