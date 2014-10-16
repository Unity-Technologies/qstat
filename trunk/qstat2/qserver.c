/*
 * qstat
 * by Steve Jankowski
 *
 * qserver functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "qstat.h"
#include "qserver.h"
#include "debug.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

// TODO: get rid of this and use send_packet instead, remove n_requests hack from a2s
query_status_t qserver_send_initial(struct qserver* server, const char* data, size_t len)
{
    int status = INPROGRESS;

    if( data )
    {
		int ret;
		debug( 2, "[%s] send", server->type->type_prefix );
		if( 4 <= get_debug_level() )
		{
			output_packet( server, data, len, 1 );
		}

		if ( server->flags & FLAG_BROADCAST)
		{
			ret = send_broadcast(server, data, len);
		}
		else
		{
			ret = send( server->fd, data, len, 0);
		}

		if ( ret == SOCKET_ERROR)
		{
			send_error( server, ret );
			status = SYS_ERROR;
		}
    }

    if ( server->retry1 == n_retries || server->flags & FLAG_BROADCAST)
    {
		gettimeofday( &server->packet_time1, NULL);
    }
    else
    {
		server->n_retries++;
    }

    server->retry1--;
    server->n_packets++;

    return status;
}

query_status_t qserver_send(struct qserver* server, const char* data, size_t len)
{
    int status = INPROGRESS;

    if(data)
    {
		int ret;
		if ( server->flags & FLAG_BROADCAST)
		{
			ret = send_broadcast(server, data, len);
		}
		else
		{
			ret = send( server->fd, data, len, 0);
		}

		if ( ret == SOCKET_ERROR)
		{
			send_error( server, ret );
			status = SYS_ERROR;
		}
    }

    server->retry1 = n_retries-1;
    gettimeofday( &server->packet_time1, NULL);
    server->n_requests++;
    server->n_packets++;

    return status;

}

int send_broadcast( struct qserver *server, const char *pkt, size_t pktlen)
{
    struct sockaddr_in addr;
    addr.sin_family= AF_INET;
    if ( no_port_offset || server->flags & TF_NO_PORT_OFFSET)
	{
        addr.sin_port= htons(server->port);
	}
    else
	{
        addr.sin_port= htons((unsigned short)( server->port + server->type->port_offset ));
	}
    addr.sin_addr.s_addr= server->ipaddr;
    memset( &(addr.sin_zero), 0, sizeof(addr.sin_zero));

    return sendto( server->fd, (const char*) pkt, pktlen, 0, (struct sockaddr *) &addr, sizeof(addr));
}

int register_send( struct qserver *server )
{
    if ( server->retry1 == n_retries || server->flags & FLAG_BROADCAST )
	{
		server->n_requests++;
    }
    else
	{
		server->n_retries++;
	}

	// New request so reset the sent time. This ensures
	// that we record an accurate ping time even on retry
	gettimeofday( &server->packet_time1, NULL);
    server->retry1--;
    server->n_packets++;

	return INPROGRESS;
}

query_status_t send_packet( struct qserver* server, const char* data, size_t len )
{
	debug( 2, "[%s] send", server->type->type_prefix );
	if( 4 <= get_debug_level() )
	{
		output_packet( server, data, len, 1 );
	}

    if( data )
    {
		int ret;
		if ( server->flags & FLAG_BROADCAST )
		{
			ret = send_broadcast( server, data, len );
		}
		else
		{
			ret = send( server->fd, data, len, 0 );
		}

		if ( ret == SOCKET_ERROR )
		{
			return send_error( server, ret );
		}
    }

	register_send( server );

	return INPROGRESS;
}

query_status_t send_error( struct qserver *server, int rc )
{
	unsigned int ipaddr = ntohl(server->ipaddr);
	const char* errstr = strerror(errno);
	fprintf(stderr, "Error on %d.%d.%d.%d: %s, skipping ...\n",
		(ipaddr >> 24) &0xff,
		(ipaddr >> 16) &0xff,
		(ipaddr >> 8) &0xff,
		ipaddr &0xff,
		errstr
	);
	return SYS_ERROR;
}
