/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Inspired by QuakePing by Len Norton
 *
 * JSON output
 * Contributed by Steve Teuber <steve@exprojects.org>
 *
 * Copyright 2013 by Steve Teuber
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "qstat.h"
#include "xform.h"
#include "display_json.h"

extern FILE *OF; /* output file */

int json_display = 0;
int json_printed = 0;

void
json_protocols()
{
	int i;
	server_type *type;
	server_type **sorted_types;

	sorted_types = (server_type **)malloc(sizeof(server_type *) * n_server_types);
	type = &types[0];
	for (i = 0; type->id != Q_UNKNOWN_TYPE && i < n_server_types; type++, i++) {
		sorted_types[i] = type;
	}

	quicksort((void **)sorted_types, 0, n_server_types - 1, (int (*)(void *, void *))type_option_compare);

	printf("{\n");
	for (i = 0; i < n_server_types; i++) {
		type = sorted_types[i];
		if (i) {
			printf(",\n");
		}
		printf("\t\"%s\": \"%s\"", type->type_string, type->game_name);
	}
	printf("\n}\n");

	exit(0);
}

void
json_version()
{
	printf("{\n\t\"version\": \"%s\"\n}\n", VERSION);
	exit(0);
}

void
json_display_server(struct qserver *server)
{
	char *prefix;
	prefix = server->type->type_string;

	if (server->server_name == DOWN) {
		if (!up_servers_only) {
			xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
			xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
			xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
			xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("offline"));
			xform_printf(OF, "\t\t\"hostname\": \"%s\"\n", json_escape((hostname_lookup) ? server->host_name : server->arg));
			xform_printf(OF, "\t}");
			json_printed = 1;
		}
		return;
	}
	if (server->server_name == TIMEOUT) {
		if (server->flags & FLAG_BROADCAST && server->n_servers) {
			xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
			xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
			xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
			xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("timeout"));
			xform_printf(OF, "\t\t\"servers\": %d\n", server->n_servers);
			xform_printf(OF, "\t}");
			json_printed = 1;
		} else if (!up_servers_only)   {
			xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
			xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
			xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
			xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("timeout"));
			xform_printf(OF, "\t\t\"hostname\": \"%s\"\n", json_escape((hostname_lookup) ? server->host_name : server->arg));
			xform_printf(OF, "\t}");
			json_printed = 1;
		}
		return;
	}

	if (server->error != NULL) {
		xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
		xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
		xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
		xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("error"));
		xform_printf(OF, "\t\t\"hostname\": \"%s\"\n", json_escape((hostname_lookup) ? server->host_name : server->arg));
		xform_printf(OF, "\t\t\"error\": \"%s\",\n", json_escape(server->error));
		xform_printf(OF, "\t}");
		json_printed = 1;
	} else if (server->type->master)   {
		xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
		xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
		xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
		xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("online"));
		xform_printf(OF, "\t\t\"servers\": %d,\n", server->n_servers);
		json_printed = 1;
	} else   {
		xform_printf(OF, (json_printed) ? ",\n\t{\n" : "\t{\n");
		xform_printf(OF, "\t\t\"protocol\": \"%s\",\n", json_escape(prefix));
		xform_printf(OF, "\t\t\"address\": \"%s\",\n", json_escape(server->arg));
		xform_printf(OF, "\t\t\"status\": \"%s\",\n", json_escape("online"));
		xform_printf(OF, "\t\t\"hostname\": \"%s\",\n", json_escape((hostname_lookup) ? server->host_name : server->arg));
		xform_printf(OF, "\t\t\"name\": \"%s\",\n", json_escape(xform_name(server->server_name, server)));
		xform_printf(OF, "\t\t\"gametype\": \"%s\",\n", json_escape(get_qw_game(server)));
		xform_printf(OF, "\t\t\"map\": \"%s\",\n", json_escape(xform_name(server->map_name, server)));
		xform_printf(OF, "\t\t\"numplayers\": %d,\n", server->num_players);
		xform_printf(OF, "\t\t\"maxplayers\": %d,\n", server->max_players);
		xform_printf(OF, "\t\t\"numspectators\": %d,\n", server->num_spectators);
		xform_printf(OF, "\t\t\"maxspectators\": %d", server->max_spectators);
		json_printed = 1;

		if (!(server->type->flags & TF_RAW_STYLE_TRIBES)) {
			xform_printf(OF, ",\n\t\t\"ping\": %d,\n", server->n_requests ? server->ping_total / server->n_requests : 999);
			xform_printf(OF, "\t\t\"retries\": %d", server->n_retries);
		}

		if (server->type->flags & TF_RAW_STYLE_QUAKE) {
			xform_printf(OF, ",\n\t\t\"address\": %s,\n", json_escape(server->address));
			xform_printf(OF, "\t\t\"protocolversion\": %d", server->protocol_version);
		}
	}

	if (!server->type->master && server->error == NULL) {
		if (get_server_rules && NULL != server->type->display_json_rule_func) {
			server->type->display_json_rule_func(server);
		}
		if (get_player_info && NULL != server->type->display_json_player_func) {
			server->type->display_json_player_func(server);
		}
	}

	xform_printf(OF, "\n\t}");
}

void
json_header()
{
	xform_printf(OF, "[\n");
}

void
json_footer()
{
	xform_printf(OF, "\n]\n");
}

void
json_display_server_rules(struct qserver *server)
{
	struct rule *rule;
	int printed = 0;
	rule = server->rules;

	xform_printf(OF, ",\n\t\t\"rules\": {\n");
	for (; rule != NULL; rule = rule->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t\"%s\": \"%s\"", json_escape(rule->name), json_escape(rule->value));
		printed = 1;
	}
	xform_printf(OF, "\n\t\t}");
}

void
json_display_q_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\"number\": %d,\n", player->number);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"address\": \"%s\",\n", json_escape(player->address));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_qw_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"number\": %d,\n", player->number);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"skin\": \"%s\",\n", player->skin ? json_escape(player->skin) : "");
		xform_printf(OF, "\t\t\t\t\"team\": \"%s\"\n", player->team_name ? json_escape(player->team_name) : "");
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_q2_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		if (server->flags & FLAG_PLAYER_TEAMS) {
			xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		}
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_player_info_info(struct player *player)
{
	struct info *info;

	for (info = player->info; info; info = info->next) {
		if (info->name) {
			char *name = json_escape(info->name);
			char *value = json_escape(info->value);
			xform_printf(OF, "\t\t\t\t\"%s\": \"%s\",\n", name, value);
		}
	}
}

void
json_display_unreal_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		if (-999 != player->deaths) {
			xform_printf(OF, "\t\t\t\t\"deaths\": %d,\n", player->deaths);
		}
		if (player->team_name != NULL) {
			xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		} else if (-1 != player->team)   {
			xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		}

		if (player->skin != NULL) {
			xform_printf(OF, "\t\t\t\t\"skin\": \"%s\",\n", player->skin ? json_escape(player->skin) : "");
		}
		if (player->mesh != NULL) {
			xform_printf(OF, "\t\t\t\t\"mesh\": \"%s\",\n", player->mesh ? json_escape(player->mesh) : "");
		}
		if (player->face != NULL) {
			xform_printf(OF, "\t\t\t\t\"face\": \"%s\",\n", player->face ? json_escape(player->face) : "");
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_halflife_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_fl_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_tribes_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"packetloss\": %d\n", player->packet_loss);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_tribes2_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;
	char *type;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->team_name != NULL) {
			switch (player->type_flag) {
			case PLAYER_TYPE_BOT:
				type = "Bot";
				break;
			case PLAYER_TYPE_ALIAS:
				type = "Alias";
				break;
			default:
				type = "";
				break;
			}

			if (printed) {
				xform_printf(OF, ",\n");
			}
			xform_printf(OF, "\t\t\t{\n");
			xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
			xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
			xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
			xform_printf(OF, "\t\t\t\t\"type\": \"%s\",\n", json_escape(type));
			xform_printf(OF, "\t\t\t\t\"clan\": \"%s\"\n", player->tribe_tag ? json_escape(xform_name(player->tribe_tag, server)) : "");
			xform_printf(OF, "\t\t\t}");
			printed = 1;
		}
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_bfris_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"number\": %d,\n", player->number);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->score);
		xform_printf(OF, "\t\t\t\t\"frags\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"ship\": %d\n", player->ship);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_descent3_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"deaths\": %d,\n", player->deaths);
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"team\": %d\n", player->team);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_ravenshield_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_ghostrecon_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"deaths\": %d,\n", player->deaths);
		xform_printf(OF, "\t\t\t\t\"team\": %d\n", player->team);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_eye_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->score);
		if (player->team_name != NULL) {
			xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		} else   {
			xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		}
		if (player->skin != NULL) {
			xform_printf(OF, "\t\t\t\t\"skin\": \"%s\",\n", json_escape(player->skin));
		}
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 1)));
		}
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_doom3_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"number\": %d,\n", player->number);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->score);
		if (player->tribe_tag != NULL) {
			xform_printf(OF, "\t\t\t\t\"clan\": \"%s\",\n", player->tribe_tag ? json_escape(xform_name(player->tribe_tag, server)) : "");
		} else   {
			xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		}
		if (player->skin != NULL) {
			xform_printf(OF, "\t\t\t\t\"skin\": \"%s\",\n", json_escape(player->skin));
		}
		if (player->type_flag != 0) {
			xform_printf(OF, "\t\t\t\t\"type\": \"%s\",\n", "bot");
		} else   {
			xform_printf(OF, "\t\t\t\t\"type\": \"%s\",\n", "player");
		}
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		if (NA_INT != player->ping) {
			xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		}
		if (NA_INT != player->score) {
			xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->score);
		}
		if (NA_INT != player->deaths) {
			xform_printf(OF, "\t\t\t\t\"deaths\": %d,\n", player->deaths);
		}
		if (NA_INT != player->frags) {
			xform_printf(OF, "\t\t\t\t\"frags\": %d,\n", player->frags);
		}
		if (player->team_name != NULL) {
			xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		} else if (NA_INT != player->team)   {
			xform_printf(OF, "\t\t\t\t\"team\": %d,\n", player->team);
		}
		if (player->skin != NULL) {
			xform_printf(OF, "\t\t\t\t\"skin\": \"%s\",\n", json_escape(player->skin));
		}
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 1)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\"\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_ts2_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_ts3_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\"\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_bfbc2_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\"\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_wic_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->score);
		xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		if (player->tribe_tag != NULL) {
			xform_printf(OF, "\t\t\t\t\"role\": \"%s\",\n", json_escape(player->tribe_tag));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"bot\": %d\n", player->type_flag);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_ventrilo_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"ping\": %d,\n", player->ping);
		xform_printf(OF, "\t\t\t\t\"team\": \"%s\",\n", json_escape(player->team_name));
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_tm_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_savage_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_farcry_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d,\n", player->frags);
		xform_printf(OF, "\t\t\t\t\"time\": \"%s\"\n", json_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_tee_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t\"score\": %d\n", player->score);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

void
json_display_starmade_player_info(struct qserver *server)
{
	struct player *player;
	int printed = 0;

	xform_printf(OF, ",\n\t\t\"players\": [\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (printed) {
			xform_printf(OF, ",\n");
		}
		xform_printf(OF, "\t\t\t{\n");
		xform_printf(OF, "\t\t\t\t\"name\": \"%s\",\n", json_escape(xform_name(player->name, server)));
		if (player->connect_time != 0) {
			xform_printf(OF, "\t\t\t\t\"time\": \"%s\",\n", json_escape(play_time(player->connect_time, 2)));
		}
		json_display_player_info_info(player);
		xform_printf(OF, "\t\t\t\t\"ping\": %d\n", player->ping);
		xform_printf(OF, "\t\t\t}");
		printed = 1;
	}

	xform_printf(OF, "\n\t\t]");
}

char
*
json_escape(char *string)
{
	static unsigned char _buf[4][MAXSTRLEN + 8];
	static int _buf_index = 0;
	unsigned char *result, *b, *end;
	unsigned int c;

	if (string == NULL) {
		return "";
	}

	result = &_buf[_buf_index][0];
	_buf_index = (_buf_index + 1) % 4;

	end = &result[MAXSTRLEN];

	b = result;
	for (; *string && b < end; string++) {
		c = *string;
		switch (c) {
		case '"':
			*b++ = '\\';
			*b++ = '"';
			continue;
		case '\\':
			*b++ = '\\';
			*b++ = '\\';
			continue;
		default:
			break;
		}

		// Validate character
		// http://www.w3.org/TR/2000/REC-xml-20001006#charsets
		if (!
			(
				0x09 == c ||
				0xA == c ||
				0xD == c ||
				(0x20 <= c && 0xD7FF >= c) ||
				(0xE000 <= c && 0xFFFD >= c) ||
				(0x10000 <= c && 0x10FFFF >= c)
			)
			) {
			if (show_errors) {
				fprintf(stderr, "Encoding error (%d) for U+%x, D+%d\n", 1, c, c);
			}
		}
		// JSON is always unicode-encoded, see RFC 4627
		// https://www.ietf.org/rfc/rfc4627.txt
		else {
			unsigned char tempbuf[10] =
			{
				0
			};
			unsigned char *buf = &tempbuf[0];
			int bytes = 0;
			int error = 1;

			// Valid character ranges
			if (
				0x09 == c ||
				0xA == c ||
				0xD == c ||
				(0x20 <= c && 0xD7FF >= c) ||
				(0xE000 <= c && 0xFFFD >= c) ||
				(0x10000 <= c && 0x10FFFF >= c)
				) {
				error = 0;
			}

			if (c < 0x80) {
				/* 0XXX XXXX one byte */
				buf[0] = c;
				bytes = 1;
			} else if (c < 0x0800)   {
				/* 110X XXXX two bytes */
				buf[0] = 0xC0 | (0x03 & (c >> 6));
				buf[1] = 0x80 | (0x3F & c);
				bytes = 2;
			} else if (c < 0x10000)   {
				/* 1110 XXXX three bytes */
				buf[0] = 0xE0 | (c >> 12);
				buf[1] = 0x80 | ((c >> 6) & 0x3F);
				buf[2] = 0x80 | (c & 0x3F);

				bytes = 3;
				if (c == UTF8BYTESWAPNOTACHAR || c == UTF8NOTACHAR) {
					error = 3;
				}
			} else if (c < 0x10FFFF)   {
				/* 1111 0XXX four bytes */
				buf[0] = 0xF0 | (c >> 18);
				buf[1] = 0x80 | ((c >> 12) & 0x3F);
				buf[2] = 0x80 | ((c >> 6) & 0x3F);
				buf[3] = 0x80 | (c & 0x3F);
				bytes = 4;
				if (c > UTF8MAXFROMUCS4) {
					error = 4;
				}
			} else if (c < 0x4000000)   {
				/* 1111 10XX five bytes */
				buf[0] = 0xF8 | (c >> 24);
				buf[1] = 0x80 | (c >> 18);
				buf[2] = 0x80 | ((c >> 12) & 0x3F);
				buf[3] = 0x80 | ((c >> 6) & 0x3F);
				buf[4] = 0x80 | (c & 0x3F);
				bytes = 5;
				error = 5;
			} else if (c < 0x80000000)   {
				/* 1111 110X six bytes */
				buf[0] = 0xFC | (c >> 30);
				buf[1] = 0x80 | ((c >> 24) & 0x3F);
				buf[2] = 0x80 | ((c >> 18) & 0x3F);
				buf[3] = 0x80 | ((c >> 12) & 0x3F);
				buf[4] = 0x80 | ((c >> 6) & 0x3F);
				buf[5] = 0x80 | (c & 0x3F);
				bytes = 6;
				error = 6;
			} else   {
				error = 7;
			}

			if (error) {
				int i;
				fprintf(stderr, "UTF-8 encoding error (%d) for U+%x, D+%d : ", error, c, c);
				for (i = 0; i < bytes; i++) {
					fprintf(stderr, "0x%02x ", buf[i]);
				}
				fprintf(stderr, "\n");
			} else   {
				int i;
				for (i = 0; i < bytes; ++i) {
					*b++ = buf[i];
				}
			}
		}
	}
	*b = '\0';
	return (char *)result;
}
