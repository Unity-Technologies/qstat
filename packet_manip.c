/*
 * qstat 2.8
 * by Steve Jankowski
 *
 * Packet module
 * Copyright 2005 Steven Hartland based on code by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "qstat.h"

#include <stdlib.h>
#include <stdio.h>

int combine_packets( struct qserver *server )
{
    unsigned int ids[8];
    int maxes[8];
    int counts[8];
    int lengths[8];
    SavedData * segments[8][16];
    SavedData *sdata= & server->saved_data;
    int n_ids= 0, i, p, done= 0;

    memset( &segments[0][0], 0, sizeof(segments));
    memset( &counts[0], 0, sizeof(counts));
    memset( &lengths[0], 0, sizeof(lengths));

    for ( ; sdata != NULL; sdata= sdata->next)
	{
		//fprintf( stderr, "COMB1: %d, %d\n", sdata->pkt_max, sdata->pkt_id );
		if ( sdata->pkt_max == 0)
		{
			continue;
		}
		for (i= 0; i < n_ids; i++)
		{
			if ( sdata->pkt_id == ids[i])
			{
				break;
			}
		}
		if ( i >= n_ids)
		{
			if ( n_ids >= 8)
			{
				continue;
			}
			ids[n_ids]= sdata->pkt_id;
			maxes[n_ids]= sdata->pkt_max;
			i= n_ids++;
		}
		else if ( maxes[i] != sdata->pkt_max)
		{
			continue;
		}

		if ( segments[i][sdata->pkt_index] == NULL)
		{
			segments[i][sdata->pkt_index]= sdata;
			counts[i]++;
			lengths[i]+= sdata->datalen;
		}
    }
	//fprintf( stderr, "COMB2: %d\n", n_ids );
    for ( i= 0; i < n_ids; i++)
    {
		char *combined;
		int datalen= 0;
		//fprintf( stderr, "COMB3: %d != %d\n", counts[i], maxes[i] );
		if ( counts[i] != maxes[i])
		{
			continue;
		}

		combined= (char*)malloc( lengths[i]);
		for ( p= 0; p < counts[i]; p++)
		{
			if ( segments[i][p] == NULL)
			{
				break;
			}
			memcpy( combined + datalen, segments[i][p]->data,
			segments[i][p]->datalen);
			datalen+= segments[i][p]->datalen;
		}
		//fprintf( stderr, "COMB4: %d != %d\n", p, counts[i] );
		if ( p < counts[i])
		{
			free( combined);
			continue;
		}

		for ( p= 0; p < counts[i]; p++)
		{
			segments[i][p]->pkt_max= 0;
		}

		//fprintf( stderr, "COMB5: callback\n" );
		done= ( (int (*)()) server->type->packet_func)( server, combined, datalen);
		free( combined);
		if ( done || server->saved_data.data == NULL)
		{
			break;
		}
    }
    return done;
}
