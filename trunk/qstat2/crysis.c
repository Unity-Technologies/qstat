/*
 * qstat
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

char *decode_crysis_val( char *val )
{
	// Very basic html conversion
	val = str_replace( val, "&quot;", "\"" );
	return str_replace( val, "&amp;", "&" );
}

query_status_t send_crysis_request_packet( struct qserver *server )
{
	char cmd[256], buf[1024], *password, *md5;
	debug( 2, "challenge: %ld", server->challenge );
	switch ( server->challenge )
	{
	case 0:
		// Not seen a challenge yet, request it
		server->challenge++;
		sprintf( cmd, "challenge" );
		break;

	case 1:
		server->challenge++;
		password = get_param_value( server, "password", "" );
		sprintf( cmd, "%s:%s", server->challenge_string, password );
		md5 = md5_hex( cmd, strlen( cmd ) );
		sprintf( cmd, "authenticate %s", md5 );
		free( md5 );
		break;

	case 2:
		// NOTE: we currently don't support player info
		server->challenge++;
		server->flags |= TF_STATUS_QUERY;
		server->n_servers = 3;
		sprintf( cmd, "status" );
		break;

	case 3:
		return DONE_FORCE;
	}

	server->saved_data.pkt_max = -1;
	sprintf(buf, "POST /RPC2 HTTP/1.1\015\012Keep-Alive: 300\015\012User-Agent: qstat %s\015\012Content-Length: %d\015\012Content-Type: text/xml\015\012\015\012<?xml version=\"1.0\" encoding=\"UTF-8\"?><methodCall><methodName>%s</methodName><params /></methodCall>", VERSION, (int)(98 + strlen(cmd)), cmd);

	return send_packet( server, buf, strlen( buf ) );
}

query_status_t valid_crysis_response( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s;
	int len;
	int cnt = packet_count( server );
	if ( 0 == cnt && 0 != strncmp( "HTTP/1.1 200 OK", rawpkt, 15 ) )
	{
		// not valid response
		return REQ_ERROR;
	}

	s = strnstr(rawpkt, "Content-Length: ", pktlen );
	if ( NULL == s )
	{
		// not valid response
		return INPROGRESS;
	}
	s += 16;
	if ( 1 != sscanf( s, "%d", &len ) )
	{
		return INPROGRESS;
	}

	s = strnstr(rawpkt, "\015\012\015\012", pktlen );
	if ( NULL == s )
	{
		return INPROGRESS;
	}

	s += 4;
	if ( pktlen != ( s - rawpkt + len ) )
	{
		return INPROGRESS;
	}

	return DONE_FORCE;
}

char* crysis_response( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *e;
	int len = pktlen;

	s = strnstr(rawpkt, "<methodResponse><params><param><value><string>", len );
	if ( NULL == s )
	{
		// not valid response
		return NULL;
	}
	s += 46;
	len += rawpkt - s;
	e = strnstr(s, "</string></value>", len );
	if ( NULL == e )
	{
		// not valid response
		return NULL;
	}
	*e = '\0';

	return strdup( s );
}

query_status_t deal_with_crysis_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *s, *val, *line;
	query_status_t state = INPROGRESS;
	debug( 2, "processing..." );

	if ( ! server->combined )
	{
		state = valid_crysis_response( server, rawpkt, pktlen );
		server->retry1 = n_retries;
		if ( 0 == server->n_requests )
		{
			server->ping_total = time_delta( &packet_recv_time, &server->packet_time1 );
			server->n_requests++;
		}

		switch ( state )
		{
		case INPROGRESS:
			{
			// response fragment recieved
			int pkt_id;
			int pkt_max;

			// We're expecting more to come
			debug( 5, "fragment recieved..." );
			pkt_id = packet_count( server );
			pkt_max = pkt_id++;
			if ( ! add_packet( server, 0, pkt_id, pkt_max, pktlen, rawpkt, 1 ) )
			{
				// fatal error e.g. out of memory
				return MEM_ERROR;
			}

			// combine_packets will call us recursively
			return combine_packets( server );
			}
		case DONE_FORCE:
			break; // single packet response fall through
		default:
			return state;
		}
	}

	if ( DONE_FORCE != state )
	{
		state = valid_crysis_response( server, rawpkt, pktlen );
		switch ( state )
		{
		case DONE_FORCE:
			break; // actually process
		default:
			return state;
		}
	}

	debug( 3, "packet: challenge = %ld", server->challenge );
	s = NULL;
	switch ( server->challenge )
	{
	case 1:
		s = crysis_response( server, rawpkt, pktlen );
		if ( NULL != s )
		{
			server->challenge_string = s;
			return send_crysis_request_packet( server );
		}
		return REQ_ERROR;
	case 2:
		s = crysis_response( server, rawpkt, pktlen );
		if ( NULL == s )
		{
			return REQ_ERROR;
		}
		if ( 0 != strncmp( s, "authorized", 10 ) )
		{
			free( s );
			return REQ_ERROR;
		}
		free( s );
		return send_crysis_request_packet( server );
	case 3:
		s = crysis_response( server, rawpkt, pktlen );
		if ( NULL == s )
		{
			return REQ_ERROR;
		}
	}

	// Correct ping
	// Not quite right but gives a good estimate
	server->ping_total = ( server->ping_total * server->n_requests ) / 2;

	debug( 3, "processing response..." );

	s = decode_crysis_val( s );
	line = strtok( s, "\012" );

	// NOTE: id=XXX and msg=XXX will be processed by the mod following the one they where the response of
	while ( NULL != line )
	{
		debug( 4, "LINE: %s\n", line );
		val = strstr( line, ":" );
		if ( NULL != val )
		{
			*val = '\0';
			val+=2;
			debug( 4, "var: %s, val: %s", line, val );
			if ( 0 == strcmp( "name", line ) )
			{
				server->server_name = strdup( val );
			}
			else if ( 0 == strcmp( "level", line ) )
			{
				server->map_name = strdup( val );
			}
			else if ( 0 == strcmp( "players", line ) )
			{
				if ( 2 == sscanf( val, "%d/%d", &server->num_players, &server->max_players) )
				{
				}
			}
			else if (
				0 == strcmp( "version", line ) ||
				0 == strcmp( "gamerules", line ) ||
				0 == strcmp( "time remaining", line )
			)
			{
				add_rule( server, line, val, NO_FLAGS );
			}
		}
		
		line = strtok( NULL, "\012" );
	}

	gettimeofday( &server->packet_time1, NULL );

	return DONE_FORCE;
}

