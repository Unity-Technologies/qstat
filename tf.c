/*
 * qstat
 *
 * Titanfall Protocol
 * Copyright 2015 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include "debug.h"
#include "qstat.h"
#include "utils.h"

#include <stdio.h>
#ifndef _WIN32
 #include <netinet/in.h>
#else
 #include <winsock.h>
#endif
#include <inttypes.h>
#include <string.h>

#define SERVERINFO_REQUEST_PAD		77
#define SERVERINFO_REQUEST		79
#define SERVERINFO_RESPONSE		80
#define SERVERINFO_VERSION		1
#define SERVERINFO_VERSION_KEYED	5
#define TEAM_IMC			"imc"
#define TEAM_MILITIA			"militia"
#define TEAM_UNKNOWN			"unknown"
#define SERVER_NAME			"unknown"

static char serverinfo_pkt[] = { 0xFF, 0xFF, 0xFF, 0xFF, SERVERINFO_REQUEST, SERVERINFO_VERSION };

static void
pkt_inc(char **pkt, int *rem, int inc)
{
	*pkt += inc;
	*rem -= inc;
}


static query_status_t
pkt_string(struct qserver *server, char **pkt, int *rem, char **str, char *rule)
{
	size_t len;

	if (*rem < 0) {
		malformed_packet(server, "short packet string");
		return (PKT_ERROR);
	}

	*str = strndup(*pkt, *rem);
	if (*str == NULL) {
		malformed_packet(server, "out of memory");
		return (MEM_ERROR);
	}

	len = strlen(*str) + 1;
	pkt_inc(pkt, rem, (int)len);

	if (rule != NULL) {
		// TODO: check return when it doesn't exit on failure
		add_rule(server, rule, *str, NO_VALUE_COPY);
	}

	return (INPROGRESS);
}


static query_status_t
pkt_rule(struct qserver *server, char **pkt, int *rem, char *rule)
{
	char *str;

	return (pkt_string(server, pkt, rem, &str, rule));
}


static query_status_t
pkt_data(struct qserver *server, char **pkt, int *rem, void *data, char *rule, int size, int isfloat)
{
	if (*rem - size < 0) {
		malformed_packet(server, "short packet data");
		return (PKT_ERROR);
	}

	memcpy(data, *pkt, size);
	pkt_inc(pkt, rem, size);

	if (rule != NULL) {
		char buf[64];

		switch (size) {
		case 1:
			sprintf(buf, "%hhu", *(uint8_t*)data);
			break;

		case 2:
			sprintf(buf, "%hu", *(uint16_t*)data);
			break;

		case 4:
			if (isfloat) {
				sprintf(buf, "%f", *(float*)data);
			} else {
				sprintf(buf, "%" PRIu32, *(uint32_t*)data);
			}
			break;

		case 8:
			sprintf(buf, "%" PRIu64, *(uint64_t*)data);
			break;
		}
		add_rule(server, rule, buf, 0);
	}

	return (INPROGRESS);
}


static query_status_t
pkt_byte(struct qserver *server, char **pkt, int *rem, uint8_t *data, char *rule)
{
	return (pkt_data(server, pkt, rem, (void *)data, rule, sizeof(*data), 0));
}


static query_status_t
pkt_short(struct qserver *server, char **pkt, int *rem, uint16_t *data, char *rule)
{
	return (pkt_data(server, pkt, rem, (void *)data, rule, sizeof(*data), 0));
}


static query_status_t
pkt_long(struct qserver *server, char **pkt, int *rem, uint32_t *data, char *rule)
{
	return (pkt_data(server, pkt, rem, (void *)data, rule, sizeof(*data), 0));
}


static query_status_t
pkt_longlong(struct qserver *server, char **pkt, int *rem, uint64_t *data, char *rule)
{
	return (pkt_data(server, pkt, rem, (void *)data, rule, sizeof(*data), 0));
}

static query_status_t
pkt_float(struct qserver *server, char **pkt, int *rem, float *data, char *rule)
{
	return (pkt_data(server, pkt, rem, (void *)data, rule, sizeof(*data), 1));
}

query_status_t
send_tf_request_packet(struct qserver *server)
{
	char *key, buf[1200];
	size_t len;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, serverinfo_pkt, sizeof(serverinfo_pkt));
	len = sizeof(serverinfo_pkt);
	if (server->type->status_packet != NULL) {
		// Custom packet type
		len = server->type->status_len;
		memcpy(buf, server->type->status_packet, len);

		if (buf[4] == SERVERINFO_REQUEST_PAD) {
			len = sizeof(buf);
		}
	}
	key = get_param_value(server, "key", NULL);
	if (key != NULL) {
		buf[5] = SERVERINFO_VERSION_KEYED;
		(void)strncpy(&buf[6], key, sizeof(buf) - 7);
		// strncpy doesn't guarantee to NULL terminate and strlcpy isn't portable.
		buf[sizeof(buf) - 1] = '\0';
		if (len != sizeof(buf)) {
			len += strlen(key) + 1;
		}
	}
	return (qserver_send_initial(server, buf, len));
}


query_status_t
deal_with_tf_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *pkt, buf[64];
	query_status_t ret;
	int rem;
	uint8_t ver, tmpu8;
	uint16_t port, tmpu16;
	uint32_t tmpu32;
	uint64_t tmpu64;
	float tmpf;

	rem = pktlen;
	pkt = rawpkt;

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		server->n_requests++;
	}

	if (pktlen < 26) {
		malformed_packet(server, "packet too short pkt");
		return (PKT_ERROR);
	}

	// Header (int32)
	if (memcmp(serverinfo_pkt, pkt, sizeof(int32_t)) != 0) {
		malformed_packet(server, "missing / invalid header");
		return (PKT_ERROR);
	}
	pkt_inc(&pkt, &rem, sizeof(int32_t));

	// Command (int8)
	debug(2, "TF type = %hhu", *pkt);
	if (server->type->status_packet != NULL) {
		if (*pkt != server->type->status_packet[4] + 1) {
			malformed_packet(server, "unknown custom type");
			return (PKT_ERROR);
		}
	} else if (*pkt != SERVERINFO_RESPONSE) {
		malformed_packet(server, "unknown type");
		return (PKT_ERROR);
	}
	pkt_inc(&pkt, &rem, sizeof(int8_t));

	// Version (byte)
	debug(2, "TF version = %hhu", *pkt);
	ret = pkt_byte(server, &pkt, &rem, &ver, "version");
	if (ret < 0) {
		return (ret);
	}

	if (ver > 1) {
		// Retail (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "retail");
		if (ret < 0) {
			return (ret);
		}

		// Instance Type (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "instance_type");
		if (ret < 0) {
			return (ret);
		}

		// Client DLL CRC (long)
		ret = pkt_long(server, &pkt, &rem, &tmpu32, "client_dll_crc");
		if (ret < 0) {
			return (ret);
		}

		// Net Prototcol (short)
		ret = pkt_short(server, &pkt, &rem, &tmpu16, "net_protocol");
		if (ret < 0) {
			return (ret);
		}

		// Random ServerID (long long)
		ret = pkt_longlong(server, &pkt, &rem, &tmpu64, "random_serverid");
		if (ret < 0) {
			return (ret);
		}

		// Build Name (string)
		ret = pkt_rule(server, &pkt, &rem, "build_name");
		if (ret < 0) {
			return (ret);
		}

		// Datacenter (string)
		ret = pkt_rule(server, &pkt, &rem, "datacenter");
		if (ret < 0) {
			return (ret);
		}

		// Game Mode (string)
		ret = pkt_rule(server, &pkt, &rem, "game_mode");
		if (ret < 0) {
			return (ret);
		}
	}

	// Port (short)
	ret = pkt_short(server, &pkt, &rem, &port, "port");
	if (ret < 0) {
		return (ret);
	}
	change_server_port(server, port, 0);

	// Platform (string)
	ret = pkt_rule(server, &pkt, &rem, "platform");
	if (ret < 0) {
		return (ret);
	}

	// Playlist Version (string)
	ret = pkt_rule(server, &pkt, &rem, "playlist_version");
	if (ret < 0) {
		return (ret);
	}

	// Playlist Num (long)
	ret = pkt_long(server, &pkt, &rem, &tmpu32, "playlist_num");
	if (ret < 0) {
		return (ret);
	}

	// Playlist Name (string)
	ret = pkt_rule(server, &pkt, &rem, "playlist_name");
	if (ret < 0) {
		return (ret);
	}

	// Num Clients (byte)
	ret = pkt_byte(server, &pkt, &rem, (uint8_t *)&server->num_players, NULL);
	if (ret < 0) {
		return (ret);
	}

	// Max Clients (byte)
	ret = pkt_byte(server, &pkt, &rem, (uint8_t *)&server->max_players, NULL);
	if (ret < 0) {
		return (ret);
	}

	// Map Name (string)
	ret = pkt_string(server, &pkt, &rem, &server->map_name, NULL);
	if (ret < 0) {
		return (ret);
	}

	if (ver > 4) {
		ret = pkt_float(server, &pkt, &rem, &tmpf, "avg_frame_time");
		if (ret < 0) {
			return (ret);
		}

		ret = pkt_float(server, &pkt, &rem, &tmpf, "max_frame_time");
		if (ret < 0) {
			return (ret);
		}

		ret = pkt_float(server, &pkt, &rem, &tmpf, "avg_user_cmd_time");
		if (ret < 0) {
			return (ret);
		}

		ret = pkt_float(server, &pkt, &rem, &tmpf, "max_user_cmd_time");
		if (ret < 0) {
			return (ret);
		}
	}

	if (ver > 2) {
		// Phase (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "phase");
		if (ret < 0) {
			return (ret);
		}

		// Max Rounds (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "max_rounds");
		if (ret < 0) {
			return (ret);
		}

		// Rounds Won IMC (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "rounds_won_imc");
		if (ret < 0) {
			return (ret);
		}

		// Rounds Won Militia (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, "rounds_won_militia");
		if (ret < 0) {
			return (ret);
		}

		// Time Limit Sec (short)
		ret = pkt_short(server, &pkt, &rem, &tmpu16, "time_limit_sec");
		if (ret < 0) {
			return (ret);
		}

		// Time Passed Sec (short)
		ret = pkt_short(server, &pkt, &rem, &tmpu16, "time_passed_sec");
		if (ret < 0) {
			return (ret);
		}

		// Max Score (short)
		ret = pkt_short(server, &pkt, &rem, &tmpu16, "max_score");
		if (ret < 0) {
			return (ret);
		}

		// Team (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, NULL);
		if (ret < 0) {
			return (ret);
		}

		while (tmpu8 != 255) {
			// Team Score (short)
			ret = pkt_short(server, &pkt, &rem, &tmpu16, NULL);
			if (ret < 0) {
				return (ret);
			}
			sprintf(buf, "%hu", tmpu16);
			switch (tmpu8) {
			case 2:
				add_rule(server, "imc_score", buf, 0);
				add_rule(server, "imc_teamid", "2", 0);
				break;

			case 3:
				add_rule(server, "militia_score", buf, 0);
				add_rule(server, "militia_teamid", "3", 0);
				break;
			}

			// Next team (byte)
			ret = pkt_byte(server, &pkt, &rem, &tmpu8, NULL);
			if (ret < 0) {
				return (ret);
			}
		}
	}

	// Client ID or EOP (uint64)
	ret = pkt_longlong(server, &pkt, &rem, &tmpu64, NULL);
	if (ret < 0) {
		return (ret);
	}

	while (tmpu64 > 0) {
		struct player *p;

		p = add_player(server, server->n_player_info);
		if (p == NULL) {
			return (SYS_ERROR);
		}

		p->flags = PLAYER_FLAG_DO_NOT_FREE_TEAM;
		sprintf(buf, "%" PRIu64, tmpu64);
		player_add_info(p, "id", buf, 0);

		// Client Name (string)
		ret = pkt_string(server, &pkt, &rem, &p->name, NULL);
		if (ret < 0) {
			return (ret);
		}

		// Client Team ID (byte)
		ret = pkt_byte(server, &pkt, &rem, &tmpu8, NULL);
		if (ret < 0) {
			return (ret);
		}
		p->team = tmpu8;
		switch (tmpu8) {
		case 2:
			p->team_name = TEAM_IMC;
			break;

		case 3:
			p->team_name = TEAM_MILITIA;
			break;

		default:
			p->team_name = TEAM_UNKNOWN;
			break;
		}
		sprintf(buf, "%hhu", tmpu8);
		player_add_info(p, "teamid", buf, 0);

		if (ver > 3) {
			// Client Address (string)
			ret = pkt_string(server, &pkt, &rem, &p->address, NULL);
			if (ret < 0) {
				return (ret);
			}

			// Client Ping (long)
			ret = pkt_long(server, &pkt, &rem, &tmpu32, NULL);
			if (ret < 0) {
				return (ret);
			}
			p->ping = tmpu32;

			// Client Incoming Packets Received (long)
			ret = pkt_long(server, &pkt, &rem, &tmpu32, NULL);
			if (ret < 0) {
				return (ret);
			}
			sprintf(buf, "%" PRIu32, tmpu32);
			player_add_info(p, "incoming_packets_received", buf, 0);

			// Client Incoming Packets Dropped (long)
			ret = pkt_long(server, &pkt, &rem, &tmpu32, NULL);
			if (ret < 0) {
				return (ret);
			}
			sprintf(buf, "%" PRIu32, tmpu32);
			player_add_info(p, "incoming_packets_dropped", buf, 0);
		}

		if (ver > 2) {
			// Client Score (long)
			ret = pkt_long(server, &pkt, &rem, &tmpu32, NULL);
			if (ret < 0) {
				return (ret);
			}
			p->score = tmpu32;

			// Client Kills (short)
			ret = pkt_short(server, &pkt, &rem, &tmpu16, NULL);
			if (ret < 0) {
				return (ret);
			}
			p->frags = tmpu16;

			// Client Deaths (short)
			ret = pkt_short(server, &pkt, &rem, &tmpu16, NULL);
			if (ret < 0) {
				return (ret);
			}
			p->deaths = tmpu16;
		}

		// Client ID or EOP (uint64)
		ret = pkt_longlong(server, &pkt, &rem, &tmpu64, NULL);
		if (ret < 0) {
			return (ret);
		}
	}

	// Protocol doesn't support server name
	server->server_name = strdup(SERVER_NAME);

	// Protocol doesn't support a specific rule request
	server->next_rule = NULL;

	return (DONE_AUTO);
}


// vim: sw=4 ts=4 noet
