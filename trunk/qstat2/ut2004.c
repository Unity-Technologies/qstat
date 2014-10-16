/*
 * qstat
 * by Steve Jankowski
 *
 * UT2004 master query functions
 * Copyright 2004 Ludwig Nussel
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 * This code is inspired by ideas from 'Nurulwai'
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
#include "md5.h"

/** \brief convert bytes into hex string
 *
 * \param in data to convert
 * \param len length of data
 * \param out location to store string to. Must be 2*len
 */
static void bin2hex(const char* in, size_t len, char* out);

#define CD_KEY_LENGTH	 23

// arbitrary
#define MAX_LISTING_RECORD_LEN 0x04FF

#define RESPONSE_OFFSET_CDKEY	 5
#define RESPONSE_OFFSET_CHALLENGE 39

static const char challenge_response[] = {
    0x68, 0x00, 0x00, 0x00,  '!', 0xCD, 0xCD, 0xCD,
    /* length             |   !   MD5SUM, CD is placeholder */
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0x00,
                                         '!', 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
    0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0x00,

    0x0c,  'U',  'T',  '2',  'K',  '4',  'C',  'L',
    /*^^ 12 byte string  */
     'I',  'E',  'N',  'T', 0x00, 0xfb, 0x0c, 0x00,
    /*                          | unknown        */
    0x00, 0x06, 0x04,  'i',  'n',  't', 0x00, 0x00,
    /*        |   ^^ 4 byte string          | ?  */
    0x00, 0x00, 0x00, 0xee, 0xee, 0x00, 0x00, 0x11,
    /* unknown                                   */
    0x00, 0x00, 0x00, 0x01 };

static const char approved[] = {
    0x0e, 0x00, 0x00, 0x00, 0x09,  'A',  'P',  'P',
     'R',  'O',  'V',  'E',  'D', 0x00, 0x03, 0x00,
    0x00, 0x00 };

static const char approved_response[] = {
    0x22, 0x00, 0x00, 0x00,  '!',  '0',  '0',  '0',
     '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',
     '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',
     '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',
     '0',  '0',  '0',  '0',  '0', 0x00 };

static const char verified[] = {
    0x0a, 0x00, 0x00, 0x00, 0x09,  'V',  'E',  'R',
     'I',  'F',  'I',  'E',  'D', 0x00 };

#if 0
struct server_listing_record_head {
	unsigned len;
	unsigned ip;
	short port;
	short queryport;
	char name[];
	// char map[]
};

struct server_listing_record_foot {
	unsigned char marker1[3];
	unsigned char unknown1;
	unsigned char maxplayers;
	unsigned char unknown2[4];
	unsigned char marker2[3];
};
#endif

static char cdkey[CD_KEY_LENGTH+1] = "";

enum ut2004_state {
	STATE_CHALLENGE = 0x00,
	STATE_APPROVED  = 0x01,
	STATE_VERIFIED  = 0x02,
	STATE_LISTING   = 0x03,
};

query_status_t send_ut2004master_request_packet(struct qserver *server)
{
	int ret;
	if(server->n_packets)
	{
		return DONE_FORCE;
	}

	if(!*cdkey)
	{
		char* param = get_param_value( server, "cdkey", NULL);
		if(!param)
		{
			debug(0, "Error: missing cdkey parameter");
			server->server_name = SYSERROR;
			return PKT_ERROR;
		}

		if(*param == '/')
		{
			FILE* fp = fopen(param, "r");
			if(!fp || fread(cdkey, 1, CD_KEY_LENGTH, fp) != CD_KEY_LENGTH)
			{
				debug(0, "Error: can't key from %s", param);
				server->server_name = SYSERROR;
				if(fp)
				{
					fclose(fp);
				}
				return PKT_ERROR;
			}
			fclose(fp);
		}
		else if(strchr(param, '-') && strlen(param) == CD_KEY_LENGTH)
		{
			memcpy(cdkey, param, CD_KEY_LENGTH);
		}
		else if(   *param == '$'
			&& (param = getenv(param+1)) // replaces param!
			&& strlen(param) == CD_KEY_LENGTH)
		{
			memcpy(cdkey, param, CD_KEY_LENGTH);
		}
		else
		{
			debug(0, "Error: invalid cdkey parameter");
			server->server_name = SYSERROR;
			return PKT_ERROR;
		}
	}

	ret = qserver_send(server, NULL, 0);

#if 0
	// XXX since we do not send but rather expect a reply directly after
	// connect it's pointless to retry doing nothing
	debug(0, "retry1: %d", server->retry1);
	server->retry1 = 0;
#endif

	server->master_query_tag[0] = STATE_CHALLENGE;
	return ret;
}

static void ut2004_server_done(struct qserver* server)
{
	if(server->saved_data.next)
	{
		debug(0, "%d bytes of unprocessed data left. Premature EOF!?", server->saved_data.next->datalen);
		free(server->saved_data.next->data);
		free(server->saved_data.next);
		server->saved_data.next = NULL;
	}
}

// we use n_servers to store number of used bytes in master_pkt so
// it needs to be divided by 6 when finished
static void ut2004_parse_record(struct qserver* server, char* pkt)
{
	char* dest;

#if 0
	unsigned ip;
	unsigned short port;

	memcpy(&ip, pkt+4, 4);
	port = swap_short_from_little(pkt+4+4);

	debug(2, "got %d.%d.%d.%d:%hu", ip&0xff, (ip>>8)&0xff, (ip>>16)&0xff, (ip>>24)&0xff, port);
#endif

	if(server->n_servers+6 > server->master_pkt_len)
	{
		if(!server->master_pkt_len)
		{
			server->master_pkt_len = 180;
		}
		else
		{
			server->master_pkt_len *= 2;
		}
		server->master_pkt = (char*)realloc(server->master_pkt, server->master_pkt_len);
	}

	dest = server->master_pkt + server->n_servers;

	memcpy(dest, pkt+4, 4 );
	dest[4] = pkt[9];
	dest[5] = pkt[8];
	server->n_servers += 6;
}

static char* put_bytes(char* buf, const char* bytes, size_t len, size_t* left)
{
	if(!buf || len > *left)
	return NULL;

	memcpy(buf, bytes, len);
	*left -= len;

	return buf+len;
}

static char* put_string(char* buf, const char* string, size_t* left)
{
	size_t len = strlen(string)+1;
	char l;

	if(!buf || len > 0xFF || *left < len+1)
		return NULL;

	l = len;

	buf = put_bytes(buf, &l, 1, left);
	return put_bytes(buf, string, len, left);
}

/** \brief assemble the server filter and send the master query

  the query consists of four bytes length (excluding the four length bytes), a
  null byte and then the number of item pairs that follow.

  Each pair consists of two ut2 strings (length+null terminated string)
  followed by a byte which is either zero or 0x04 which means negate the query
  (e.g. not zero curplayers means not empty).
 */
static int ut2004_send_query(struct qserver* server)
{
	char buf[4096] = {0};
	size_t left = sizeof(buf);
	char *b = buf;
	char *param, *r, *sep= "";
	unsigned flen = 0;
	unsigned char items = 0;

	// header is done later
	b += 6;
	left -= 6;

	param = get_param_value( server, "gametype", NULL);
	if(param)
	{
		++items;
		b = put_string(b, "gametype", &left);
		b = put_string(b, param, &left);
		b = put_bytes(b, "", 1, &left);
	}

	param = get_param_value( server, "status", NULL);
	r = param;
	while ( param && sep )
	{
		sep= strchr( r, ':');
		if ( sep )
			flen= sep-r;
		else
			flen= strlen(r);

		if (   strncmp( r, "standard", flen) == 0
			|| strncmp( r, "nostandard", flen) == 0)
		{
			++items;
			b = put_string(b, "standard", &left);
			if(*r == 'n')
			b = put_string(b, "false", &left);
			else
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		else if (  strncmp( r, "password", flen) == 0
			|| strncmp( r, "nopassword", flen) == 0)
		{
			++items;
			b = put_string(b, "password", &left);
			if(*r == 'n')
			b = put_string(b, "false", &left);
			else
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		else if ( strncmp( r, "notempty", flen) == 0)
		{
			++items;
			b = put_string(b, "currentplayers", &left);
			b = put_string(b, "0", &left);
			b = put_bytes(b, "\x04", 1, &left);
		}
		else if ( strncmp( r, "notfull", flen) == 0)
		{
			++items;
			b = put_string(b, "freespace", &left);
			b = put_string(b, "0", &left);
			b = put_bytes(b, "\x04", 1, &left);
		}
		else if ( strncmp( r, "nobots", flen) == 0)
		{
			++items;
			b = put_string(b, "nobots", &left);
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		else if (  strncmp( r, "stats", flen) == 0
			|| strncmp( r, "nostats", flen) == 0)
		{
			++items;
			b = put_string(b, "stats", &left);
			if(*r == 'n')
			b = put_string(b, "false", &left);
			else
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		else if (  strncmp( r, "weaponstay", flen) == 0
			|| strncmp( r, "noweaponstay", flen) == 0)
		{
			++items;
			b = put_string(b, "weaponstay", &left);
			if(*r == 'n')
			b = put_string(b, "false", &left);
			else
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		else if (  strncmp( r, "transloc", flen) == 0
			|| strncmp( r, "notransloc", flen) == 0)
		{
			++items;
			b = put_string(b, "transloc", &left);
			if(*r == 'n')
			b = put_string(b, "false", &left);
			else
			b = put_string(b, "true", &left);
			b = put_bytes(b, "", 1, &left);
		}
		r= sep+1;
	}

	param = get_param_value( server, "mutator", NULL);
	r = param;
	sep = "";
	while ( param && sep )
	{
		char neg = '\0';
		unsigned char l;
		sep= strchr( r, ':');
		if ( sep )
			flen= sep-r;
		else
			flen= strlen(r);

		if(*r == '-')
		{
			neg = '\x04';
			++r;
			--flen;
		}

		if(!flen)
			continue;

		b = put_string(b, "mutator", &left);
		l = flen+1;
		b = put_bytes(b, (char*)&l, 1, &left);
		b = put_bytes(b, r, flen, &left);
		b = put_bytes(b, "", 1, &left);
		b = put_bytes(b, &neg, 1, &left);
		++items;

		r= sep+1;
	}

	if(!b)
	{
		debug(0, "Error: query buffer too small. Please file a bug report!");
		return 0;
	}

	put_long_little(b-buf-4, buf);
	buf[5] = items;

	return (qserver_send(server, buf, sizeof(buf)-left) > 0);
}

query_status_t deal_with_ut2004master_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned char* state = (unsigned char*)&server->master_query_tag[0];

	md5_state_t md5;

	if(!pktlen)
	{
		ut2004_server_done(server);
		goto cleanup_out;
	}

	server->ping_total+= time_delta( &packet_recv_time, &server->packet_time1);

	switch(*state)
	{
	case STATE_CHALLENGE:
		// ensure at least one byte challenge, fit into buffer,
		// match challenge, null terminated
		if(	pktlen < 4 +1 +1 +1
		|| pktlen > 4 +1 +8 +1
		|| rawpkt[pktlen-1] != '\0')
		{
			malformed_packet(server, "invalid challenge" );
			goto cleanup_out;
		}
		else
		{
			char response[sizeof(challenge_response)];
			char* challenge = rawpkt+5;
			char sum[16];

			memcpy(response, challenge_response, sizeof(challenge_response));

			debug(2, "challenge: %s", challenge);

			md5_init(&md5);
			md5_append(&md5, (unsigned char*)cdkey, CD_KEY_LENGTH);
			md5_finish(&md5, (unsigned char*)sum);
			bin2hex(sum, 16, response+RESPONSE_OFFSET_CDKEY);

			md5_init(&md5);
			md5_append(&md5, (unsigned char*)cdkey, CD_KEY_LENGTH);
			md5_append(&md5, (unsigned char*)challenge, strlen(challenge));
			md5_finish(&md5, (unsigned char*)sum);
			bin2hex(sum, 16, response+RESPONSE_OFFSET_CHALLENGE);

			if(qserver_send(server, response, sizeof(response)))
				goto cleanup_out;

			server->server_name = MASTER;

			*state = STATE_APPROVED;
		}
		break;

	case STATE_APPROVED:

		if(pktlen != sizeof(approved)
		|| 0 != memcmp(rawpkt, approved, pktlen))
		{
			malformed_packet(server, "CD key not approved" );
			goto cleanup_out;
		}

		debug(2, "got approval, sending verify");

		if(qserver_send(server, approved_response, sizeof(approved_response)))
			goto cleanup_out;

		*state = STATE_VERIFIED;

		break;

	case STATE_VERIFIED:

		if(pktlen != sizeof(verified)
		|| 0 != memcmp(rawpkt, verified, pktlen))
		{
			malformed_packet(server, "CD key not verified" );
			goto cleanup_out;
		}

		debug(2, "CD key verified, sending query");

		if(ut2004_send_query(server))
			goto cleanup_out;

		*state = STATE_LISTING;

		break;
	case STATE_LISTING:
		// first packet. contains number of servers to expect
		if(!server->saved_data.pkt_id)
		{
		/*
			server->saved_data.data = malloc(pktlen);
			memcpy(server->saved_data.data, rawpkt, pktlen);
			server->saved_data.datalen = pktlen;
			*/
			server->saved_data.pkt_id = 1;

			if(pktlen == 9)
			{
				unsigned num = swap_long_from_little(rawpkt+4);
				debug(2, "expect %u servers", num);
#if 1
				if(num < 10000)
				{
					server->master_pkt_len = num*6;
					server->master_pkt = (char*)realloc(server->master_pkt, server->master_pkt_len);
				}
#endif
			}
		}
		else if(pktlen < 4)
		{
			malformed_packet(server, "packet too short");
			goto cleanup_out;
		}
		else
		{
			char* p = rawpkt;
			unsigned recordlen = 0;

			if(server->saved_data.next)
			{
				unsigned need = 0;
				SavedData* data = server->saved_data.next;
				// nasty, four bytes of record length are split up. since
				// we alloc'ed at least four bytes we just copy the 4-x
				// bytes to data->data
				if(data->datalen < 4)
				{
					need = 4 - data->datalen;
					debug(2, "need %d bytes more for recordlen", need);
					if( need > pktlen)
					{
						// XXX ok, im lazy now. Stupid server can't even
						// send four bytes in a row
						malformed_packet(server, "chunk too small");
						goto cleanup_out;
					}
					memcpy(data->data+data->datalen, p, need);
					p += need;
					data->datalen = 4;
				}

				recordlen = swap_long_from_little(data->data);

				if(!recordlen || recordlen > MAX_LISTING_RECORD_LEN)
				{
					malformed_packet(server,
						"record lengthx %x out of range, position %d", recordlen, (int)(p-rawpkt));
					goto cleanup_out;
				}

				need = 4+recordlen - data->datalen;

				debug(2, "recordlen: %d, saved: %d, pkglen: %d, needed: %d", recordlen, data->datalen, pktlen, need);

				if( need <= pktlen)
				{
					data->data = realloc(data->data, 4+recordlen);
					memcpy(data->data + data->datalen, p, need);
					ut2004_parse_record(server, data->data);
					p += need;

					free(data->data);
					free(data);
					server->saved_data.next = NULL;
				}
			}

			while(!server->saved_data.next && p-rawpkt+4 < pktlen)
			{
				recordlen = swap_long_from_little(p);

				// record too large
				if(!recordlen || recordlen > MAX_LISTING_RECORD_LEN)
				{
					malformed_packet(server,
						"record length %x out of range, position %d", recordlen, (int)(p-rawpkt));
					goto cleanup_out;
				}
				// recordlen itself is four bytes
				recordlen += 4;

				// record fully inside packet
				if(p-rawpkt+recordlen <= pktlen)
				{
					ut2004_parse_record(server, p);
					p += recordlen;
				}
				else
					break;
			}

			// record continues in next packet. save it.
			if(p-rawpkt < pktlen)
			{
				SavedData* data = server->saved_data.next;
				unsigned tosave = pktlen - (p-rawpkt);
				if(!data)
				{
					data = malloc(sizeof(SavedData));
					data->data = malloc(tosave<4?4:tosave); // alloc at least four bytes
					data->datalen = tosave;
					memcpy(data->data, p, data->datalen);
					data->next = NULL;
					server->saved_data.next = data;

					debug(1, "saved %d bytes", data->datalen );
				}
				else
				{
					data->data = realloc(data->data, data->datalen + tosave );
					memcpy(data->data+data->datalen, p, tosave);
					data->datalen += tosave;

					debug(1, "saved %d bytes (+)", data->datalen );
				}
			}
		}
		break;
	}

	debug(2, "%d servers total", server->n_servers/6);

	return INPROGRESS;

cleanup_out:
	server->master_pkt_len = server->n_servers;
	server->n_servers /= 6;
	return DONE_FORCE;
}

static const char hexchar[] = "0123456789abcdef";

static void bin2hex(const char* in, size_t len, char* out)
{
	char* o = out+len*2;
	in += len;
	do
	{
		*--o = hexchar[*--in&0x0F];
		*--o = hexchar[(*in>>4)&0x0F];
	} while(o != out);
}

// vim: sw=4 ts=4 noet
