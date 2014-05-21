/*
 * qstat 2.14
 *
 * opentTTD protocol
 * Copyright 2007 Ludwig Nussel
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
#include "qserver.h"
#include "debug.h"

enum { MAX_VEHICLE_TYPES = 5, MAX_STATION_TYPES = 5 };

static const char* vehicle_types[] = {
	"num_trains",
	"num_trucks",
	"num_busses",
	"num_aircrafts",
	"num_ships",
};

static const char* station_types[] = {
	"num_stations",
	"num_truckbays",
	"num_busstations",
	"num_airports",
	"num_docks",
};

query_status_t deal_with_ottdmaster_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned num;
	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	server->server_name = MASTER;

	if(swap_short_from_little(rawpkt) != pktlen)
	{
		malformed_packet( server, "invalid packet length" );
		return PKT_ERROR;
	}
	if(rawpkt[2] != 7)
	{
		malformed_packet( server, "invalid packet type" );
		return PKT_ERROR;
	}

	if(rawpkt[3] != 1)
	{
		malformed_packet( server, "invalid packet version" );
		return PKT_ERROR;
	}

	num = swap_short_from_little(&rawpkt[4]);
	rawpkt += 6;
	pktlen -= 6;
	if( num && num*6 <= pktlen )
	{
		unsigned i;
		server->master_pkt = (char*)realloc(server->master_pkt, server->master_pkt_len + pktlen );
		memset(server->master_pkt + server->master_pkt_len, 0, pktlen );
		server->master_pkt_len += pktlen;
		for( i = 0; i < num * 6; i += 6 )
		{
			memcpy(&server->master_pkt[i], &rawpkt[i], 4);
			server->master_pkt[i+4] = rawpkt[i+5];
			server->master_pkt[i+5] = rawpkt[i+4];
		}
		server->n_servers += num;
	}
	else
	{
		malformed_packet( server, "invalid packet" );
		return PKT_ERROR;
	}

	bind_sockets();

	return DONE_AUTO;
}

#define xstr(s) str(s)
#define str(s) #s

#define GET_STRING do { \
		str = (char*)ptr; \
		ptr = memchr(ptr, '\0', end-ptr); \
		if ( !ptr ) \
		{ \
			malformed_packet( server, "%s:%s invalid packet", __FILE__, xstr(__LINE__) ); \
			return PKT_ERROR; \
		} \
		++ptr; \
	} while(0)

#define FAIL_IF(cond, msg) \
	if((cond)) { \
		malformed_packet( server, "%s:%s %s", __FILE__, xstr(__LINE__), msg ); \
		return PKT_ERROR; \
	}

#define INVALID_IF(cond) \
	FAIL_IF(cond, "invalid packet")

query_status_t deal_with_ottd_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned char *ptr = (unsigned char*)rawpkt;
	unsigned char *end = (unsigned char*)(rawpkt + pktlen);
	unsigned char type;
	char* str;
	char buf[32];
	unsigned ver;

	server->n_servers++;
	if ( server->server_name == NULL)
	{
		server->ping_total += time_delta( &packet_recv_time, &server->packet_time1);
		server->n_requests++;
	}
	else
	{
		gettimeofday( &server->packet_time1, NULL);
	}

	FAIL_IF(pktlen < 4 || swap_short_from_little(rawpkt) > pktlen, "invalid packet");

	type = ptr[2];
	ver = ptr[3];
	ptr += 4;

	debug(3, "len %hu type %hhu ver %u", swap_short_from_little(rawpkt), type, ver);

	FAIL_IF(ver != 4 && ver != 5, "only version 4 and 5 servers are supported");

	if(type == 1) // info packet
	{
		unsigned numgrf = *ptr;
		FAIL_IF(ptr + numgrf * 20 + 1 > end, "invalid newgrf number");
		ptr += numgrf * 20 + 1;

		snprintf(buf, sizeof(buf), "%u", swap_long_from_little(ptr));
		add_rule(server, "date_days", buf, NO_FLAGS);
		ptr += 4;

		snprintf(buf, sizeof(buf), "%u", swap_long_from_little(ptr));
		add_rule(server, "startdate_days", buf, NO_FLAGS);
		ptr += 4;

		FAIL_IF(ptr + 3 > end, "invalid packet");

		snprintf(buf, sizeof(buf), "%hhu", ptr[0]);
		add_rule(server, "maxcompanies", buf, NO_FLAGS);
		snprintf(buf, sizeof(buf), "%hhu", ptr[1]);
		add_rule(server, "numcompanies", buf, NO_FLAGS);
		server->max_spectators = ptr[2];
		ptr += 3;

		GET_STRING;
		server->server_name = strdup(str);

		GET_STRING;
		add_rule(server, "version", str, NO_FLAGS);

		FAIL_IF(ptr + 7 > end, "invalid packet");

		{
			static const char* langs[] = {
				"any",
				"English",
				"German",
				"French"
			};
			unsigned i = *ptr++;
			if(i > 3) i = 0;
			add_rule(server, "language", (char*)langs[i], NO_FLAGS);
		}

		add_rule(server, "password", *ptr++ ? "1" : "0", NO_FLAGS);

		server->max_players = *ptr++;
		server->num_players = *ptr++;
		server->num_spectators = *ptr++;

		GET_STRING;

		server->map_name = strdup(str);

		snprintf(buf, sizeof(buf), "%hu", swap_short_from_little(ptr));
		add_rule(server, "map_width", buf, NO_FLAGS);
		snprintf(buf, sizeof(buf), "%hu", swap_short_from_little(ptr));
		add_rule(server, "map_height", buf, NO_FLAGS);

		{
			static const char* sets[] = {
				"temperate",
				"arctic",
				"desert",
				"toyland"
			};
			unsigned i = *ptr++;
			if(i > 3) i = 0;
			add_rule(server, "map_set", (char*)sets[i], NO_FLAGS);
		}

		add_rule(server, "dedicated", *ptr++ ? "1" : "0", NO_FLAGS);
	}
	else if(type == 3) // player packet
	{
		unsigned i, j;
		INVALID_IF(ptr + 2 > end);

		server->num_players = *ptr++;

		for(i = 0; i < server->num_players; ++i)
		{
			unsigned long long lli;
			struct player* player;
			unsigned char nr;

			nr = *ptr++;

			debug(3, "player number %d", nr);
			player = add_player(server, i);
			FAIL_IF(!player, "can't allocate player");

			GET_STRING;
			player->name = strdup(str);
			debug(3, "name %s", str);
			player->frags = 0;

			INVALID_IF(ptr + 4 + 3*8 + 2 + 1 + 2*MAX_VEHICLE_TYPES + 2*MAX_STATION_TYPES > end);

			snprintf(buf, sizeof(buf), "%u", swap_long_from_little(ptr));
			player_add_info(player, "startdate", buf, 0);
			ptr += 4;

			lli = swap_long_from_little(ptr+4);
			lli <<= 32;
			lli += swap_long_from_little(ptr);
			snprintf(buf, sizeof(buf), "%lld", lli);
			player_add_info(player, "value", buf, 0);
			ptr += 8;

			lli = swap_long_from_little(ptr+4);
			lli <<= 32;
			lli = swap_long_from_little(ptr);
			snprintf(buf, sizeof(buf), "%lld", lli);
			player_add_info(player, "money", buf, 0);
			ptr += 8;

			lli = swap_long_from_little(ptr+4);
			lli <<= 32;
			lli += swap_long_from_little(ptr);
			snprintf(buf, sizeof(buf), "%lld", lli);
			player_add_info(player, "income", buf, 0);
			ptr += 8;

			snprintf(buf, sizeof(buf), "%hu", swap_short_from_little(ptr));
			player_add_info(player, "performance", buf, 0);
			ptr += 2;

			player_add_info(player, "password", *ptr?"1":"0", 0);
			++ptr;

			for (j = 0; j < MAX_VEHICLE_TYPES; ++j)
			{
				snprintf(buf, sizeof(buf), "%hu", swap_short_from_little(ptr));
				player_add_info(player, (char*)vehicle_types[j], buf, 0);
				ptr += 2;
			}
			for (j = 0; j < MAX_STATION_TYPES; ++j)
			{
				snprintf(buf, sizeof(buf), "%hu", swap_short_from_little(ptr));
				player_add_info(player, (char*)station_types[j], buf, 0);
				ptr += 2;
			}
			
			if (ver != 5)
			{
				// connections
				while(ptr + 1 < end && *ptr)
				{
					++ptr;
					GET_STRING; // client name
					debug(3, "%s played by %s", str, player->name);
					GET_STRING; // id
					INVALID_IF(ptr + 4 > end);
					ptr += 4;
				}

				++ptr; // record terminated by zero byte
			}
		}

		// spectators
		while(ptr + 1 < end && *ptr)
		{
			++ptr;
			GET_STRING; // client name
			debug(3, "spectator %s", str);
			GET_STRING; // id
			INVALID_IF(ptr + 4 > end);
			ptr += 4;
		}
		++ptr; // record terminated by zero byte

		server->next_rule = NO_SERVER_RULES; // we're done
		server->next_player_info = server->num_players; // we're done
	}
	else
	{
		malformed_packet( server, "invalid type" );
		return PKT_ERROR;
	}

	server->retry1 = n_retries; // we're done with this packet, reset retry counter

	return DONE_AUTO;
}

query_status_t send_ottdmaster_request_packet(struct qserver *server)
{
	return qserver_send_initial(server, server->type->master_packet, server->type->master_len);
}

query_status_t send_ottd_request_packet(struct qserver *server)
{
	qserver_send_initial(server, server->type->status_packet, server->type->status_len);

	if(get_server_rules || get_player_info)
	{
		server->next_rule = ""; // trigger calling send_a2s_rule_request_packet
	}

	return INPROGRESS;
}
