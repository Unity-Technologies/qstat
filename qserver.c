/*
 * qstat 2.6
 * by Steve Jankowski
 *
 * qserver functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "qstat.h"
#include "qserver.h"

#ifdef _ISUNIX
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int qserver_send_initial(struct qserver* server, const char* data, size_t len)
{
    int ret = 0;

    if(data)
    {
	if ( server->flags & FLAG_BROADCAST)
	    ret = send_broadcast(server, data, len);
	else
	    ret = send( server->fd, data, len, 0);

	if ( ret == SOCKET_ERROR)
	{
	    perror( "send");
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

    return ret;
}

int qserver_send(struct qserver* server, const char* data, size_t len)
{
    int ret = 0;

    if(data)
    {
	if ( server->flags & FLAG_BROADCAST)
	    ret = send_broadcast(server, data, len);
	else
	    ret = send( server->fd, data, len, 0);

	if ( ret == SOCKET_ERROR)
	{
	    perror( "send");
	}
    }

    server->retry1 = n_retries;
    gettimeofday( &server->packet_time1, NULL);
    server->n_requests++;
    server->n_packets++;

    return ret;

}

int
send_broadcast( struct qserver *server, const char *pkt, size_t pktlen)
{
    struct sockaddr_in addr;
    addr.sin_family= AF_INET;
    if ( no_port_offset || server->flags & TF_NO_PORT_OFFSET)
        addr.sin_port= htons(server->port);
    else
        addr.sin_port= htons((unsigned short)( server->port + server->type->port_offset ));
    addr.sin_addr.s_addr= server->ipaddr;
    memset( &(addr.sin_zero), 0, sizeof(addr.sin_zero));
    return sendto( server->fd, (const char*) pkt, pktlen, 0,
		(struct sockaddr *) &addr, sizeof(addr));
}
