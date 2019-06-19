/*
 * simple zandronum query - no master stuff
 * Copyright 2018 DayenTech
 *
 */

#include <sys/types.h>
#ifndef _WIN32
  #include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "zandronum.h"
#include "skulltag_huffman.h"

#include "debug.h"

// global get_player_info

/*
 * Query flags from https://wiki.zandronum.com/Launcher_protocol
 */
#define SQF_NAME 	0x00000001 /* 	The name of the server */
#define SQF_URL 	0x00000002 /* 	The associated website */
#define SQF_EMAIL 	0x00000004 /* 	Contact address */
#define SQF_MAPNAME 	0x00000008 /* 	Current map being played */
#define SQF_MAXCLIENTS 	0x00000010 /* 	Maximum amount of clients who can connect to the server */
#define SQF_MAXPLAYERS 	0x00000020 /* 	Maximum amount of players who can join the game (the rest must spectate) */
#define SQF_PWADS 	0x00000040 /* 	PWADs loaded by the server */
#define SQF_GAMETYPE 	0x00000080 /* 	Game type code */
#define SQF_GAMENAME 	0x00000100 /* 	Game mode name */
#define SQF_IWAD 	0x00000200 /* 	The IWAD used by the server */
#define SQF_FORCEPASSWORD 	0x00000400 /* 	Whether or not the server enforces a password */
#define SQF_FORCEJOINPASSWORD 	0x00000800 /* 	Whether or not the server enforces a join password */
#define SQF_GAMESKILL 	0x00001000 /* 	The skill level on the server */
#define SQF_BOTSKILL 	0x00002000 /* 	The skill level of any bots on the server */
#define SQF_DMFLAGS 	0x00004000 /* 	(Deprecated) The values of dmflags, dmflags2 and compatflags. Use SQF_ALL_DMFLAGS instead. */
#define SQF_LIMITS 	0x00010000 /* 	Timelimit, fraglimit, etc. */
#define SQF_TEAMDAMAGE 	0x00020000 /* 	Team damage factor. */
#define SQF_TEAMSCORES 	0x00040000 /* 	(Deprecated) The scores of the red and blue teams. Use SQF_TEAMINFO_* instead. */
#define SQF_NUMPLAYERS 	0x00080000 /* 	Amount of players currently on the server. */
#define SQF_PLAYERDATA 	0x00100000 /* 	Information of each player in the server. */
#define SQF_TEAMINFO_NUMBER 	0x00200000 /* 	Amount of teams available. */
#define SQF_TEAMINFO_NAME 	0x00400000 /* 	Names of teams. */
#define SQF_TEAMINFO_COLOR 	0x00800000 /* 	RGB colors of teams. */
#define SQF_TEAMINFO_SCORE 	0x01000000 /* 	Scores of teams. */
#define SQF_TESTING_SERVER 	0x02000000 /* 	Whether or not the server is a testing server, also the name of the testing binary. */
#define SQF_DATA_MD5SUM 	0x04000000 /* 	(Deprecated) Used to retrieve the MD5 checksum of skulltag_data.pk3, now obsolete and returns an empty string instead. */
#define SQF_ALL_DMFLAGS 	0x08000000 /* 	Values of various dmflags used by the server. */
#define SQF_SECURITY_SETTINGS 	0x10000000 /* 	Security setting values (for now only whether the server enforces the master banlist) */
#define SQF_OPTIONAL_WADS 	0x20000000 /* 	Which PWADs are optional */
#define SQF_DEH 	0x40000000 /* 	List of DEHACKED patches loaded by the server.  */

enum ZANDRONUM_GAMEMODE {
    GAMEMODE_COOPERATIVE,
    GAMEMODE_SURVIVAL,
    GAMEMODE_INVASION,
    GAMEMODE_DEATHMATCH,
    GAMEMODE_TEAMPLAY,
    GAMEMODE_DUEL,
    GAMEMODE_TERMINATOR,
    GAMEMODE_LASTMANSTANDING,
    GAMEMODE_TEAMLMS,
    GAMEMODE_POSSESSION,
    GAMEMODE_TEAMPOSSESSION,
    GAMEMODE_TEAMGAME,
    GAMEMODE_CTF,
    GAMEMODE_ONEFLAGCTF,
    GAMEMODE_SKULLTAG,
    GAMEMODE_DOMINATION
};

static const uint32_t zandronum_base_flags = (SQF_NAME | SQF_MAPNAME | SQF_MAXPLAYERS | SQF_GAMETYPE | SQF_GAMENAME | SQF_NUMPLAYERS);

typedef struct zandronum_server_query_s {
    uint32_t challenge; // 199
    uint32_t flags;
    uint32_t time;
    uint32_t flags2; // 0 - for SQF_EXTENDED_INFO
} zandronum_server_query_t;

static bool huffman_constructed = false;

char* build_zandronum_query_packet(char* query_buf, int* packet_len) {
    if (!huffman_constructed) {
        HUFFMAN_Construct();
        huffman_constructed = true;
    }
    zandronum_server_query_t query;
    memset(&query, 0, sizeof(query));
    query.challenge = 199;
    query.flags = zandronum_base_flags;
    if (get_player_info) { // global
        query.flags |= SQF_PLAYERDATA;
    }
    HUFFMAN_Encode((unsigned char*)&query, (unsigned char*)query_buf, sizeof(query), packet_len);
    return query_buf;
}

query_status_t send_zandronum_request_packet(struct qserver *server) {
    int rc = 0;
    int packet_len = -1;
    char *packet = NULL;
    char query_buf[4096] = { 0 };

    server->next_player_info = NO_PLAYER_INFO;

    packet_len = sizeof(query_buf);
    packet = build_zandronum_query_packet(query_buf, &packet_len);

    rc = send(server->fd, packet, packet_len, 0);
    if (rc == SOCKET_ERROR) {
        return (send_error(server, rc));
    }

    if (server->retry1 == n_retries) {
        gettimeofday(&server->packet_time1, NULL);
        server->n_requests++;
    } else {
        server->n_retries++;
    }
    server->retry1--;
    server->n_packets++;

    return (INPROGRESS);
}

typedef struct zandronum_response_header_s {
    uint32_t response; // 5660023 accepted, ..24 denied to many requests, ..25 banned
    uint32_t time; // waste of bytes
    // ...
} zandronum_response_header_t;

static char* zandronum_skip_string(char* start, char* end) {
    for (; start < end; start++) {
        if (start[0] == 0) {
            break;
        }
    }
    return start+1;
}

query_status_t deal_with_zandronum_packet(struct qserver *server, char *rawpkt, int pktlen) {
    static char packet_buffer[4096];
    zandronum_response_header_t response_header;
    char* version;
    char* ptr = packet_buffer;
    char* end;
    uint32_t flags;
    uint32_t expected_flags;
    int decode_len;
    int game_mode = GAMEMODE_COOPERATIVE;
    struct player* z_player = (struct player*)NULL;
    struct player* n_player = (struct player*)NULL;
    uint16_t points;
    uint16_t ping;

    HUFFMAN_Decode((unsigned char*)rawpkt, (unsigned char*)&packet_buffer[0], pktlen, &decode_len);
    if ((decode_len >= sizeof(packet_buffer)) || (decode_len < sizeof(zandronum_response_header_t))) {
        debug(2, "bad decode length: %d\n", decode_len);
        return PKT_ERROR;
    }
    end = ptr + decode_len;

    memcpy(&response_header, packet_buffer, sizeof(response_header));
    if (response_header.response != 5660023) {
        debug(2, "bad response: %d\n", response_header.response);
        return PKT_ERROR;
    }

    server->n_servers++;
    if (server->server_name == NULL) {
        server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
    } else {
        gettimeofday(&server->packet_time1, NULL);
    }
    ptr += sizeof(response_header);
    if (ptr >= end) {
        debug(2, "response too small");
        return PKT_ERROR;
    }
    version = ptr;
    ptr = zandronum_skip_string(ptr, end);
    if (ptr >= end) {
        return PKT_ERROR;
    }
    debug(1, "Zandronum Version: %s\n", version);

    expected_flags = zandronum_base_flags;
    if (get_player_info) { // global var
        expected_flags |= SQF_PLAYERDATA;
    }
    memcpy(&flags, ptr, sizeof(flags));
    if (flags & ~expected_flags) {
        debug(2, "Bad Flags, Got %0x - Wanted %0x\n", flags, expected_flags);
        return PKT_ERROR; // server returned stuff we didn't ask for
    }
    ptr += sizeof(flags);
    if (ptr >= end) {
        return PKT_ERROR;
    }
    if (flags & SQF_NAME) {
        debug(1, "Server name: %s\n", ptr);
        server->server_name = strdup(ptr);
        ptr = zandronum_skip_string(ptr, end);
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (flags & SQF_MAPNAME) {
        debug(1, "Map name: %s\n", ptr);
        server->map_name = strdup(ptr);
        ptr = zandronum_skip_string(ptr, end);
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (flags & SQF_MAXPLAYERS) {
        server->max_players = (int)(ptr[0]);
        ptr++;
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (flags & SQF_GAMETYPE) {
        game_mode = (int)(ptr[0]);
        debug(1, "Game mode %d\n", game_mode);
        ptr += 3; // skip instagib and buckshot flags
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (flags & SQF_GAMENAME) {
        debug(1, "Game name %s\n", ptr);
        server->game = strdup(ptr);
        add_rule(server, (char*)"gametype", server->game, NO_FLAGS);
        ptr = zandronum_skip_string(ptr, end);
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (flags & SQF_NUMPLAYERS) {
        server->num_players = (int)(ptr[0]);
        debug(1, "number of players: %d\n", server->num_players);
        ptr++;
        if (ptr >= end) {
            return PKT_ERROR;
        }
    }

    if (server->num_players < 1)  {
        server->num_players = 0;
        return DONE_FORCE;
    }

    if (!(flags & SQF_PLAYERDATA)) {
        server->num_players = 0;
        return DONE_FORCE;
    }

    server->players = (struct player*)NULL;

    for (int x=0; x < server->num_players; x++) {
        n_player = (struct player*)malloc(sizeof(struct player));
        memset(n_player, 0, sizeof(struct player));
        if (server->players == (struct player*)NULL) {
            server->players = n_player;
        } else {
            z_player->next = n_player;
        }
        z_player = n_player;
        n_player->name = strdup(ptr);
        ptr = zandronum_skip_string(ptr, end);
        if (ptr >= end) {
            goto PLAYER_ERROR;
        }

        memcpy(&points, ptr, sizeof(points));
        ptr += sizeof(points);
        if (ptr >= end) {
            goto PLAYER_ERROR;
        }
        n_player->frags = points;

        memcpy(&ping, ptr, sizeof(ping));
        ptr += sizeof(ping);
        if (ptr >= end) {
            goto PLAYER_ERROR;
        }
        n_player->ping = ping;

        if ((int)ptr[0] == 1) {
            server->num_spectators++;
        }
        ptr++;
        if (ptr >= end) {
            goto PLAYER_ERROR;
        }
        if ((int)ptr[0] == 1) { // I'm a bot!
            int nb_size = strlen(n_player->name) + 7;
            char * name_buffer = (char*)malloc(nb_size);
            snprintf(name_buffer, nb_size, "bot - %s", n_player->name);
            free(n_player->name);
            n_player->name = name_buffer;
        }
        ptr++;
        if (ptr >= end) {
            goto PLAYER_ERROR;
        }
        switch (game_mode) {
            case GAMEMODE_TEAMPLAY:
            case GAMEMODE_TEAMLMS:
            case GAMEMODE_TEAMPOSSESSION:
            case GAMEMODE_TEAMGAME:
            case GAMEMODE_CTF:
            case GAMEMODE_ONEFLAGCTF:
            case GAMEMODE_SKULLTAG:
            case GAMEMODE_DOMINATION:
            { // team games have a team number
                n_player->team = (int)ptr[0];
                ptr++;
                if (ptr >= end) {
                    goto PLAYER_ERROR;
                }
            } break;
            default: break;
        }
        n_player->connect_time = (int)ptr[0] * 60; // qstat expects connect_time in seconds
        ptr++;
        if (ptr > end) {
            goto PLAYER_ERROR;
        }
        debug(1, "adding player: %s %d %d %d\n", n_player->name, n_player->frags, n_player->ping, n_player->connect_time);
    }
    return DONE_FORCE;

    PLAYER_ERROR: // error occurred while unpacking players
    debug(1, "not enough player info returned\n");
    for (z_player = server->players; z_player; z_player = n_player) {
        n_player = z_player->next;
        free(z_player);
    }
    server->players = (struct player*)NULL;
    return PKT_ERROR;
}

