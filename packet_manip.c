/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * Packet module
 * Copyright 2005 Steven Hartland based on code by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "qstat.h"
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>

#define MAX_PACKETS 8
#define MAX_FAGMENTS 128

int pkt_seq = 0;
int pkt_id_index = -1;
int n_ids;
int counts[MAX_PACKETS];
SavedData *segments[MAX_PACKETS][MAX_FAGMENTS];

int combine_packets( struct qserver *server )
{
	unsigned int ids[MAX_PACKETS];
	int maxes[MAX_PACKETS];
	int lengths[MAX_PACKETS];
	SavedData *sdata = &server->saved_data;
	int i, p, ret = INPROGRESS;
	n_ids = 0;

	memset( &segments[0][0], 0, sizeof(segments) );
	memset( &counts[0], 0, sizeof(counts) );
	memset( &lengths[0], 0, sizeof(lengths) );

	// foreach packet
	for ( ; sdata != NULL; sdata= sdata->next)
	{
		debug( 4, "max:%d, id:%d\n", sdata->pkt_max, sdata->pkt_id );
		if ( sdata->pkt_max == 0)
		{
			// not expecting multi packets or already processed?
			continue;
		}

		if ( sdata->pkt_index >= MAX_FAGMENTS )
		{
			// we only deal up to MAX_FAGMENTS packet fragment
			fprintf( stderr, "Too many fragments %d for packetid %d max %d\n", sdata->pkt_index, sdata->pkt_id, MAX_FAGMENTS );
			return PKT_ERROR;
		}

		for ( i = 0; i < n_ids; i++ )
		{
			if ( sdata->pkt_id == ids[i])
			{
				// found this packetid
				break;
			}
		}

		if ( i >= n_ids )
		{
			// packetid we havent seen yet
			if ( n_ids >= MAX_PACKETS )
			{
				// we only deal up to MAX_PACKETS packetids
				fprintf( stderr, "Too many distinct packetids %d max %d\n", n_ids, MAX_PACKETS );
				return PKT_ERROR;
			}
			ids[n_ids]= sdata->pkt_id;
			maxes[n_ids]= sdata->pkt_max;
			i = n_ids++;
		}
		else if ( maxes[i] != sdata->pkt_max )
		{
			// max's dont match
			debug( 4, "max mismatch %d != %d", maxes[i], sdata->pkt_max );
			continue;
		}

		if ( segments[i][sdata->pkt_index] == NULL)
		{
			// add the packet to the list of segments
			segments[i][sdata->pkt_index]= sdata;
			counts[i]++;
			lengths[i] += sdata->datalen;
		}
		else
		{
			debug( 2, "duplicate packet detected for id %d, index %d", sdata->pkt_id, sdata->pkt_index );
		}
	}

	// foreach distinct packetid
	for ( pkt_id_index = 0; pkt_id_index < n_ids; pkt_id_index++ )
	{
		char *combined;
		int datalen = 0;
		int combinedlen;

		if ( counts[pkt_id_index] != maxes[pkt_id_index] )
		{
			// we dont have all the expected packets yet
			debug( 4, "more expected: %d != %d\n", counts[pkt_id_index], maxes[pkt_id_index] );
			continue;
		}

		// combine all the segments
		combinedlen = lengths[pkt_id_index];
		combined = (char*)malloc(combinedlen);
		for ( p = 0; p < counts[pkt_id_index]; p++ )
		{
			if ( segments[pkt_id_index][p] == NULL )
			{
				debug( 4, "missing segment[%d][%d]", pkt_id_index, p );
				// reset to be unusable
				pkt_id_index = -1;
				free( combined );
				return INPROGRESS;
			}
			if (datalen + segments[pkt_id_index][p]->datalen > combinedlen) {
				fprintf(stderr, "Data length %d > combined length %d\n",
					datalen + segments[pkt_id_index][p]->datalen, combinedlen);
				// reset to be unusable
				pkt_id_index = -1;
				free(combined);
				return MEM_ERROR;
			}
			memcpy( combined + datalen, segments[pkt_id_index][p]->data, segments[pkt_id_index][p]->datalen );
			datalen += segments[pkt_id_index][p]->datalen;
		}

		// prevent reprocessing?
		for ( p = 0; p < counts[pkt_id_index]; p++)
		{
			segments[pkt_id_index][p]->pkt_max = 0;
		}

		debug( 4, "callback" );
		if( 4 <= get_debug_level() )
		{
			print_packet( server, combined, datalen );
		}
		// Call the server's packet processing method flagging as a combine call
		server->combined = 1;
		ret = ( (int (*)()) server->type->packet_func)( server, combined, datalen );
		free( combined );
		server->combined = 0;

		// Note: this is currently invalid as packet processing methods
		// are void not int
		if ( INPROGRESS != ret || NULL == server->saved_data.data )
		{
			break;
		}
	}
	// reset to be unusable
	pkt_id_index = -1;

	return ret;
}

// NOTE:
// pkt_id is the packet aka response identifier
// pkt_index is the index of the packet fragment
int add_packet( struct qserver *server, unsigned int pkt_id, int pkt_index, int pkt_max, int datalen, char *data, int calc_max )
{
	SavedData *sdata;

	// safety net for bad data
	if (datalen == 0) {
		debug(1, "Empty packet received!");
		return 0;
	}

	if ( server->saved_data.data == NULL )
	{
		debug( 4, "first packet: %d id, %d index, %d max, %d calc_max", pkt_id, pkt_index, pkt_max, calc_max );
		sdata = &server->saved_data;
	}
	else
	{
		debug( 4, "another packet: %d id, %d index, %d max, %d calc_max", pkt_id, pkt_index, pkt_max, calc_max );
		if ( calc_max )
		{
			// check we have the correct max
			SavedData *cdata = &server->saved_data;
			for ( ; cdata != NULL; cdata = cdata->next )
			{
				if ( cdata->pkt_max > pkt_max )
				{
					pkt_max = cdata->pkt_max;
				}
			}

			// ensure all the packets know about this new max
			for ( cdata = &server->saved_data; cdata != NULL; cdata = cdata->next )
			{
				cdata->pkt_max = pkt_max;
			}
		}
		debug( 4, "calced max = %d", pkt_max );

		// allocate a new packet data and prepend to the list
		sdata = (SavedData*) calloc( 1, sizeof(SavedData) );
		sdata->next = server->saved_data.next;
		server->saved_data.next = sdata;
	}

	sdata->pkt_id = pkt_id;
	sdata->pkt_index = pkt_index;
	sdata->pkt_max = pkt_max;
	sdata->datalen = datalen;
	sdata->data = (char*)malloc( sdata->datalen );
	if ( NULL == sdata->data )
	{
		fprintf( stderr, "Out of memory\n" );
		return 0;
	}

	memcpy( sdata->data, data, sdata->datalen );

	return 1;
}

int next_sequence()
{
	return ++pkt_seq;
}

SavedData* get_packet_fragment( int index )
{
	if ( -1 == pkt_id_index )
	{
		fprintf( stderr, "Invalid call to get_packet_fragment" );
		return NULL;
	}

	if ( index > counts[pkt_id_index] )
	{
		debug( 4, "Invalid index requested %d >  %d", index, pkt_id_index );
		return NULL;
	}

	return segments[pkt_id_index][index];
}

unsigned combined_length( struct qserver *server, int pkt_id )
{
	SavedData *sdata = &server->saved_data;
	unsigned len = 0;
	for ( ; sdata != NULL; sdata = sdata->next )
	{
		if ( pkt_id == sdata->pkt_id )
		{
			len += sdata->datalen;
		}
	}
	return len;
}

unsigned packet_count( struct qserver *server )
{
	SavedData *sdata = &server->saved_data;
	unsigned cnt = 0;
	if ( NULL == sdata->data )
	{
		return 0;
	}

	for ( ; sdata != NULL; sdata = sdata->next )
	{
		cnt++;
	}
	return cnt;
}
