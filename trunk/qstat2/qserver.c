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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int qserver_send_initial(struct qserver* server, const char* data, size_t len)
{
    int ret = 0;

    if(data)
    {
	ret = send( server->fd, data, len, 0);

	if ( ret == SOCKET_ERROR)
	{
	    perror( "send");
	}
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

    return ret;
}

int qserver_send(struct qserver* server, const char* data, size_t len)
{
    int ret = 0;

    if(data)
    {
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
