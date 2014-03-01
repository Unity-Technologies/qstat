/*
 * qstat 2.14
 * by Steve Jankowski
 *
 * Ventrilo query protocol
 * Algorithm by Luigi Auriemma <www.aluigi.org>
 * QStat port 2010 by Michael Willigens <michael@willigens.de>
 * Fixes by Steven Hartland <steven.hartland@multiplay.co.uk>
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define strtok_ret strtok_r
#define VENTRILO_RAND random()
#else
#include <winsock.h>
#define strtok_ret strtok_s
#define VENTRILO_RAND rand()
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "debug.h"
#include "qstat.h"
#include "packet_manip.h"


int VENTRILO_COMMAND_GENERIC_INFO = 1;
int VENTRILO_COMMAND_DETAILED_INFO = 2;	// includes generic infos
int VENTRILO_COMMAND_DIFFERENT = 7;

typedef struct {
	unsigned short pckkey;  // key for decoding this header
	unsigned short zero;	// ever 0
	unsigned short cmd;	 // command number: 1 generic info, 2 for details
	unsigned short id;	  // packet ID used for tracking the replies
	unsigned short totlen;  // total data size (for data splitted in packets)
	unsigned short len;	 // size of the data in this packet (max 492)
	unsigned short totpck;  // total amount of packets (max 32)
	unsigned short pck;	 // current packet number
	unsigned short datakey; // key for decoding the data
	unsigned short crc;	 // checksum of the total plain-text data
} ventrilo_udp_head;

const static unsigned char  ventrilo_udp_encdata_head[] =
	"\x80\xe5\x0e\x38\xba\x63\x4c\x99\x88\x63\x4c\xd6\x54\xb8\x65\x7e"
	"\xbf\x8a\xf0\x17\x8a\xaa\x4d\x0f\xb7\x23\x27\xf6\xeb\x12\xf8\xea"
	"\x17\xb7\xcf\x52\x57\xcb\x51\xcf\x1b\x14\xfd\x6f\x84\x38\xb5\x24"
	"\x11\xcf\x7a\x75\x7a\xbb\x78\x74\xdc\xbc\x42\xf0\x17\x3f\x5e\xeb"
	"\x74\x77\x04\x4e\x8c\xaf\x23\xdc\x65\xdf\xa5\x65\xdd\x7d\xf4\x3c"
	"\x4c\x95\xbd\xeb\x65\x1c\xf4\x24\x5d\x82\x18\xfb\x50\x86\xb8\x53"
	"\xe0\x4e\x36\x96\x1f\xb7\xcb\xaa\xaf\xea\xcb\x20\x27\x30\x2a\xae"
	"\xb9\x07\x40\xdf\x12\x75\xc9\x09\x82\x9c\x30\x80\x5d\x8f\x0d\x09"
	"\xa1\x64\xec\x91\xd8\x8a\x50\x1f\x40\x5d\xf7\x08\x2a\xf8\x60\x62"
	"\xa0\x4a\x8b\xba\x4a\x6d\x00\x0a\x93\x32\x12\xe5\x07\x01\x65\xf5"
	"\xff\xe0\xae\xa7\x81\xd1\xba\x25\x62\x61\xb2\x85\xad\x7e\x9d\x3f"
	"\x49\x89\x26\xe5\xd5\xac\x9f\x0e\xd7\x6e\x47\x94\x16\x84\xc8\xff"
	"\x44\xea\x04\x40\xe0\x33\x11\xa3\x5b\x1e\x82\xff\x7a\x69\xe9\x2f"
	"\xfb\xea\x9a\xc6\x7b\xdb\xb1\xff\x97\x76\x56\xf3\x52\xc2\x3f\x0f"
	"\xb6\xac\x77\xc4\xbf\x59\x5e\x80\x74\xbb\xf2\xde\x57\x62\x4c\x1a"
	"\xff\x95\x6d\xc7\x04\xa2\x3b\xc4\x1b\x72\xc7\x6c\x82\x60\xd1\x0d";

const static unsigned char  ventrilo_udp_encdata_data[] =
	"\x82\x8b\x7f\x68\x90\xe0\x44\x09\x19\x3b\x8e\x5f\xc2\x82\x38\x23"
	"\x6d\xdb\x62\x49\x52\x6e\x21\xdf\x51\x6c\x76\x37\x86\x50\x7d\x48"
	"\x1f\x65\xe7\x52\x6a\x88\xaa\xc1\x32\x2f\xf7\x54\x4c\xaa\x6d\x7e"
	"\x6d\xa9\x8c\x0d\x3f\xff\x6c\x09\xb3\xa5\xaf\xdf\x98\x02\xb4\xbe"
	"\x6d\x69\x0d\x42\x73\xe4\x34\x50\x07\x30\x79\x41\x2f\x08\x3f\x42"
	"\x73\xa7\x68\xfa\xee\x88\x0e\x6e\xa4\x70\x74\x22\x16\xae\x3c\x81"
	"\x14\xa1\xda\x7f\xd3\x7c\x48\x7d\x3f\x46\xfb\x6d\x92\x25\x17\x36"
	"\x26\xdb\xdf\x5a\x87\x91\x6f\xd6\xcd\xd4\xad\x4a\x29\xdd\x7d\x59"
	"\xbd\x15\x34\x53\xb1\xd8\x50\x11\x83\x79\x66\x21\x9e\x87\x5b\x24"
	"\x2f\x4f\xd7\x73\x34\xa2\xf7\x09\xd5\xd9\x42\x9d\xf8\x15\xdf\x0e"
	"\x10\xcc\x05\x04\x35\x81\xb2\xd5\x7a\xd2\xa0\xa5\x7b\xb8\x75\xd2"
	"\x35\x0b\x39\x8f\x1b\x44\x0e\xce\x66\x87\x1b\x64\xac\xe1\xca\x67"
	"\xb4\xce\x33\xdb\x89\xfe\xd8\x8e\xcd\x58\x92\x41\x50\x40\xcb\x08"
	"\xe1\x15\xee\xf4\x64\xfe\x1c\xee\x25\xe7\x21\xe6\x6c\xc6\xa6\x2e"
	"\x52\x23\xa7\x20\xd2\xd7\x28\x07\x23\x14\x24\x3d\x45\xa5\xc7\x90"
	"\xdb\x77\xdd\xea\x38\x59\x89\x32\xbc\x00\x3a\x6d\x61\x4e\xdb\x29";

const static unsigned short ventrilo_crc_table[] =
{
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

void ventrilo_udp_head_dec(unsigned char *data)
{
	int i;
	unsigned short *p;
	unsigned char a1;
	unsigned char a2;

	p = (unsigned short *)data;
	data += 2;

	*p = ntohs(*p);
	a1 = *p;
	if( ! a1 )
	{
		return;
	}

	a2 = *p >> 8;
	for( i = 0; i < 18; i++ )
	{
		data[i] -= ventrilo_udp_encdata_head[a2] + (i % 5);
		a2 += a1;
	}

	for( i = 0; i < 9; i++ )
	{
		p++;
		*p = ntohs(*p);
	}
}

void ventrilo_udp_head_enc(unsigned char *data)
{
	int i;
	unsigned short *p;
	unsigned char a1;
	unsigned char a2;

	p = (unsigned short *)data;
	data += 2;

	*p = (((VENTRILO_RAND * 0x343fd) + 0x269ec3) >> 16) & 0x7fff;
	a1 = *p;
	a2 = *p >> 8;
	if( ! a2 )
	{
		a2 = 69;
		*p |= (a2 << 8);
	}

	for( i = 0; i < 10; i++ )
	{
		*p = htons(*p);
		p++;
	}

	for( i = 0; i < 18; i++ )
	{
		data[i] += ventrilo_udp_encdata_head[a2] + (i % 5);
		a2 += a1;
	}
}

void ventrilo_udp_data_dec(unsigned char *data, int len, unsigned short key)
{
	int i;
	unsigned char a1;
	unsigned char a2;

	a1 = key;
	if( ! a1 )
	{
		return;
	}

	a2 = key >> 8;
	for( i = 0; i < len; i++ )
	{
		data[i] -= ventrilo_udp_encdata_data[a2] + (i % 72);
		a2 += a1;
	}
}

unsigned short ventrilo_udp_data_enc(unsigned char *data, int len)
{
	int i;
	unsigned short key;
	unsigned char a1;
	unsigned char a2;

	key = (((VENTRILO_RAND * 0x343fd) + 0x269ec3) >> 16) & 0x7fff;
	a1 = key;
	a2 = key >> 8;
	if( ! a2 )
	{
		a2 = 1;
		key |= (a2 << 8);
	}

	for( i = 0; i < len; i++ )
	{
		data[i] += ventrilo_udp_encdata_data[a2] + (i % 72);
		a2 += a1;
	}

	return key;
}



unsigned short ventrilo_udp_crc(unsigned char *data, int len)
{
	unsigned short  crc = 0;

	while( len-- )
	{
		crc = ventrilo_crc_table[crc >> 8] ^ *data ^ (crc << 8);
		data++;
	}

	return(crc);
}

int buildVentriloRequest(unsigned char *buf, int cmd, char *pass, int id)
{
	ventrilo_udp_head *stat = (ventrilo_udp_head *) buf;
	unsigned char *data = buf + 20;

	strncpy((char *) data, pass, 16);

	stat->zero = 0;
	stat->cmd = cmd;
	stat->id = id;
	stat->totlen = 16;
	stat->len = 16;
	stat->totpck = 1;
	stat->pck = 0;
	stat->crc = ventrilo_udp_crc(data, 16);
	stat->datakey = ventrilo_udp_data_enc(data, 16);
	ventrilo_udp_head_enc(buf);

	return 20+16;
}

query_status_t send_ventrilo_request_packet( struct qserver *server )
{
	char buf[1500];
	char *password = get_param_value( server, "password", "" );
	int size = buildVentriloRequest((unsigned char *) buf, VENTRILO_COMMAND_DETAILED_INFO, password, server->challenge);
	server->n_requests++;
	gettimeofday( &server->packet_time1, NULL );

	debug( 2, "send status request");

	return send_packet( server, buf, size );
}

query_status_t deal_with_ventrilo_packet( struct qserver *server, char *rawpkt, int pktlen )
{
	char *line, *last_line;

	debug( 3, "deal_with_ventrilo_packet: state = %ld", server->challenge );

	if( 20 > pktlen )
	{
		debug( 2, "wrong or incomplete packet received. retrying...");
		return INPROGRESS;
	}

	/* add to the data previously saved to get a full packet*/
	if ( ! server->combined )
	{
		ventrilo_udp_head *header = (ventrilo_udp_head *) rawpkt;
		unsigned char *data = (unsigned char *) header+20;
		ventrilo_udp_head_dec( (unsigned char*) header );
		ventrilo_udp_data_dec(data, header->len, header->datakey);

		if (header->id != server->challenge)
		{
			debug( 2, "ignoring unnecessary packet...");
			return INPROGRESS;
		}

		debug( 3, "decoded ventrilo fragment: id:%i - len:%i - totlen:%i - totpck:%i - zero:%i - cmd:%i", header->id, header->len, header->totlen, header->totpck, header->zero, header->cmd);

		add_packet(server, header->id, packet_count(server), header->totpck, header->len, (char *) data, 0);
		return combine_packets( server );
	}

	debug( 4, "combined ventrilo packet: %s", rawpkt);

	server->ping_total += time_delta( &packet_recv_time, &server->packet_time1 );

	line = strtok_ret( rawpkt, "\n", &last_line );
	debug( 3, "processing detailed response..." );
	while (line != NULL)
	{
		debug( 4, "processing line: %s", line );
		if ( 0 == strncmp( line, "NAME: ", 6 ) )
		{
			server->server_name = strdup( line + 6 );
		}
		else if ( 0 == strncmp( line, "MAXCLIENTS: ", 12 ) )
		{
			server->max_players = atoi( line + 12 );
		}
		else if ( 0 == strncmp( line, "CLIENTCOUNT: ", 13 ) )
		{
			server->num_players = atoi( line + 13 );
		}
		else if ( 0 == strncmp( line, "CLIENT: ", 8 ) )
		{
			// Client e.g.
			// CLIENT: UID=406,ADMIN=0,CID=17,PHAN=0,PING=73,SEC=2245,NAME=Darrimu,COMM=
			struct player *player = add_player( server, server->n_player_info );
			char *last_info = NULL;
			char *player_info = strtok_ret( line + 8, ",", &last_info );
			while ( NULL != player_info )
			{
				debug( 5, "player info: %s", player_info );
				if ( 0 ==  strncmp( player_info, "NAME=", 5 ) )
				{
					player->name = strdup( player_info + 5 );
				}
				else if ( strncmp( player_info, "PING=", 5 ) )
				{
					player->ping = atoi( player_info + 5 );
				}
				else if ( 0 == strncmp( player_info, "CID=", 4 ) )
				{
					player->team = atoi( player_info + 4 );
				}
				else if ( strncmp( player_info, "SEC=", 4 ) )
				{
					player->connect_time = atoi( player_info + 4 );
				}
				player_info = strtok_ret( NULL, ",", &last_info );
			}
		}

		line = strtok_ret( NULL, "\n", &last_line );
	}
	server->map_name = strdup( "N/A" );

	return DONE_FORCE;
}
