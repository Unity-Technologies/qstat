/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Inspired by QuakePing by Len Norton
 *
 * RAW Output
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002,2003,2004 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "display_raw.h"

int raw_arg = 0;
int show_game_in_raw = 0;
int raw_display = 0;
char *raw_delimiter = "\t";

void
raw_display_server(struct qserver *server)
{
	char *prefix;
	int ping_time;
	prefix = server->type->type_prefix;

	if (server->n_requests) {
		ping_time = server->ping_total / server->n_requests;
	} else   {
		ping_time = 999;
	}

	if (server->server_name == DOWN || server->server_name == SYSERROR) {
		if (!up_servers_only) {
			xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s\n\n",
						 prefix,
						 raw_arg, RD,
						 raw_arg,
						 server->arg, RD,
						 (hostname_lookup) ? server->host_name : server->arg, RD,
						 server->server_name
						 );
		}
		return;
	}

	if (server->server_name == TIMEOUT) {
		if (server->flags & FLAG_BROADCAST && server->n_servers) {
			xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%d\n", prefix, raw_arg, RD, raw_arg, server->arg, RD, server->arg, RD, server->n_servers);
		} else if (!up_servers_only)   {
			xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s\n\n", prefix, raw_arg, RD, raw_arg, server->arg, RD, (hostname_lookup) ? server->host_name : server->arg, RD, TIMEOUT);
		}
		return;
	}

	if (server->error != NULL) {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s" "%s%s",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 "ERROR", RD,
					 server->error
					 );
	} else if (server->type->flags & TF_RAW_STYLE_QUAKE)   {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s" "%s%s" "%s%d" "%s%s" "%s%d" "%s%d" "%s%d" "%s%d" "%s%d" "%s%d" "%s%s",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 xform_name(server->server_name, server), RD,
					 server->address, RD,
					 server->protocol_version, RD,
					 server->map_name, RD,
					 server->max_players, RD,
					 server->num_players, RD,
					 server->max_spectators, RD,
					 server->num_spectators, RD,
					 ping_time, RD,
					 server->n_retries,
					 show_game_in_raw ? RD : "",
					 show_game_in_raw ? get_qw_game(server) : ""
					 );
	} else if (server->type->flags & TF_RAW_STYLE_TRIBES)   {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s" "%s%s" "%s%d" "%s%d",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 xform_name(server->server_name, server), RD,
					 (server->map_name) ? server->map_name : "?", RD,
					 server->num_players, RD,
					 server->max_players
					 );
	} else if (server->type->flags & TF_RAW_STYLE_GHOSTRECON)   {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s" "%s%s" "%s%d" "%s%d",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 xform_name(server->server_name, server), RD,
					 (server->map_name) ? server->map_name : "?", RD,
					 server->num_players, RD,
					 server->max_players
					 );
	} else if (server->type->master)   {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%d",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 server->n_servers
					 );
	} else   {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%s" "%s%s" "%s%d" "%s%d" "%s%d" "%s%d" "%s%s",
					 prefix,
					 raw_arg, RD,
					 raw_arg,
					 server->arg, RD,
					 (hostname_lookup) ? server->host_name : server->arg, RD,
					 xform_name(server->server_name, server), RD,
					 (server->map_name) ? xform_name(server->map_name, server) : "?", RD,
					 server->max_players, RD,
					 server->num_players, RD,
					 ping_time, RD,
					 server->n_retries,
					 show_game_in_raw ? RD : "",
					 show_game_in_raw ? get_qw_game(server) : ""
					 );
	}
	fputs("\n", OF);

	if (server->type->master || server->error != NULL) {
		fputs("\n", OF);
		return;
	}

	if (get_server_rules && NULL != server->type->display_raw_rule_func) {
		server->type->display_raw_rule_func(server);
	}
	if (get_player_info && NULL != server->type->display_raw_player_func) {
		server->type->display_raw_player_func(server);
	}
	fputs("\n", OF);
}

void
raw_display_server_rules(struct qserver *server)
{
	struct rule *rule;

	int printed = 0;
	rule = server->rules;
	for (; rule != NULL; rule = rule->next) {
		if (server->type->id == TRIBES2_SERVER) {
			char *v;
			for (v = rule->value; *v; v++) {
				if (*v == '\n') {
					*v = ' ';
				}
			}
		}

		xform_printf(OF, "%s%s=%s", (printed) ? RD : "", rule->name, rule->value);
		printed++;
	}
	if (server->missing_rules) {
		xform_printf(OF, "%s?", (printed) ? RD : "");
	}
	fputs("\n", OF);
}

void
raw_display_q_player_info(struct qserver *server)
{
	char fmt[] = "%d" "%s%s" "%s%s" "%s%d" "%s%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 player->number, RD,
					 xform_name(player->name, server), RD,
					 player->address, RD,
					 player->frags, RD,
					 play_time(player->connect_time, 1), RD,
					 quake_color(player->shirt_color), RD,
					 quake_color(player->pants_color)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_qw_player_info(struct qserver *server)
{
	char fmt[128];
	struct player *player;

	strcpy(fmt, "%d" "%s%s" "%s%d" "%s%s" "%s%s" "%s%s");
	strcat(fmt, "%s%d" "%s%s" "%s%s");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 player->number, RD,
					 xform_name(player->name, server), RD,
					 player->frags, RD,
					 play_time(player->connect_time, 1), RD,
					 quake_color(player->shirt_color), RD,
					 quake_color(player->pants_color), RD,
					 player->ping, RD,
					 player->skin ? player->skin : "", RD,
					 player->team_name ? player->team_name : ""
					 );
		fputs("\n", OF);
	}
}

void
raw_display_q2_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%d";
	static const char *fmt_team = "%s" "%s%d" "%s%d" "%s%d";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (server->flags & FLAG_PLAYER_TEAMS) {
			xform_printf(OF, fmt_team, xform_name(player->name, server), RD, player->frags, RD, player->ping, RD, player->team);
		} else   {
			xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, player->ping);
		}
		fputs("\n", OF);
	}
}

void
raw_display_unreal_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%d" "%s%d" "%s%s" "%s%s" "%s%s";
	static const char *fmt_team_name = "%s" "%s%d" "%s%d" "%s%s" "%s%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, fmt_team_name,
						 xform_name(player->name, server), RD,
						 player->frags, RD,
						 player->ping, RD,
						 player->team_name, RD,
						 player->skin ? player->skin : "", RD,
						 player->mesh ? player->mesh : "", RD,
						 player->face ? player->face : ""
						 );
		} else   {
			xform_printf(OF, fmt,
						 xform_name(player->name, server), RD,
						 player->frags, RD,
						 player->ping, RD,
						 player->team, RD,
						 player->skin ? player->skin : "", RD,
						 player->mesh ? player->mesh : "", RD,
						 player->face ? player->face : ""
						 );
		}
		fputs("\n", OF);
	}
}

void
raw_display_halflife_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}

void
raw_display_fl_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s" "%s%d" "%s%d";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		fprintf(
			OF, fmt,
			xform_name(player->name, server), RD,
			player->frags, RD,
			play_time(player->connect_time, 1), RD,
			player->ping, RD,
			player->team
			);
		fputs("\n", OF);
	}
}

void
raw_display_tribes_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%d" "%s%d" "%s%d";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, player->ping, RD, player->team, RD, player->packet_loss);
		fputs("\n", OF);
	}
}

void
raw_display_tribes2_player_info(struct qserver *server)
{
	static char fmt[] = "%s" "%s%d" "%s%d" "%s%s" "%s%s" "%s%s";
	struct player *player;

	char *type;

	player = server->players;
	for (; player != NULL; player = player->next) {
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
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->frags, RD,
					 player->team, RD,
					 player->team_name ? player->team_name : "TEAM", RD,
					 type, RD,
					 player->tribe_tag ? xform_name(player->tribe_tag, server) : ""
					 );
		fputs("\n", OF);
	}
}

void
raw_display_bfris_player_info(struct qserver *server)
{
	static char fmt[] = "%d" "%s%d" "%s%s" "%s%d" "%s%d" "%s%d" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 player->number, RD,
					 player->ship, RD,
					 player->team_name, RD,
					 player->ping, RD,
					 player->score, RD,
					 player->frags, RD,
					 xform_name(player->name, server)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_descent3_player_info(struct qserver *server)
{
	static char fmt[] = "%s" "%s%d" "%s%d" "%s%d" "%s%d";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, player->deaths, RD, player->ping, RD, player->team);
		fputs("\n", OF);
	}
}

void
raw_display_ghostrecon_player_info(struct qserver *server)
{
	static char fmt[28] = "%s" "%s%d" "%s%d";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->deaths, RD, player->team);
		fputs("\n", OF);
	}
}

void
raw_display_eye_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%d" "%s%d" "%s%s" "%s%s";
	static const char *fmt_team_name = "%s" "%s%d" "%s%d" "%s%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, fmt_team_name,
						 xform_name(player->name, server), RD,
						 player->score, RD,
						 player->ping, RD,
						 player->team_name, RD,
						 player->skin ? player->skin : "", RD,
						 play_time(player->connect_time, 1)
						 );
		} else   {
			xform_printf(OF, fmt,
						 xform_name(player->name, server), RD,
						 player->score, RD,
						 player->ping, RD,
						 player->team, RD,
						 player->skin ? player->skin : "", RD,
						 play_time(player->connect_time, 1)
						 );
		}
		fputs("\n", OF);
	}
}

void
raw_display_doom3_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%d" "%s%d" "%s%u";
	static const char *fmt_team_name = "%s" "%s%d" "%s%d" "%s%s" "%s%u";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->tribe_tag) {
			xform_printf(OF, fmt_team_name, xform_name(player->name, server), RD, player->score, RD, player->ping, RD, player->tribe_tag, RD, player->number);
		} else   {
			xform_printf(OF, fmt, xform_name(player->name, server), RD, player->score, RD, player->ping, RD, player->team, RD, player->number);
		}
		fputs("\n", OF);
	}
}

void
raw_display_gs2_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%d" "%s%d" "%s%s" "%s%s";
	static const char *fmt_team_name = "%s" "%s%d" "%s%d" "%s%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, fmt_team_name, xform_name(player->name, server), RD,
						 player->score, RD,
						 player->ping, RD,
						 player->team_name, RD,
						 player->skin ? player->skin : "", RD,
						 play_time(player->connect_time, 1)
						 );
		} else   {
			xform_printf(OF, fmt,
						 xform_name(player->name, server), RD,
						 player->score, RD,
						 player->ping, RD,
						 player->team, RD,
						 player->skin ? player->skin : "", RD,
						 play_time(player->connect_time, 1)
						 );
		}
		fputs("\n", OF);
	}
}

void
raw_display_armyops_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		player->score = calculate_armyops_score(player);
	}

	raw_display_gs2_player_info(server);
}

void
raw_display_ts2_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->ping, RD,
					 player->skin ? player->skin : "", RD,
					 play_time(player->connect_time, 1)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_ts3_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->skin ? player->skin : "", RD,
					 play_time(player->connect_time, 1)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_starmade_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->skin ? player->skin : "", RD,
					 play_time(player->connect_time, 1)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_bfbc2_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->skin ? player->skin : "", RD,
					 play_time(player->connect_time, 1)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_wic_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->score, RD,
					 player->team_name, RD,
					 player->tribe_tag ? player->tribe_tag : ""
					 );
		fputs("\n", OF);
	}
}

void
raw_display_ventrilo_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		fprintf(
			OF, fmt,
			xform_name(player->name, server),
			RD, player->team,
			RD, player->team_name,
			RD, play_time(player->connect_time, 1)
			);
		fputs("\n", OF);
	}
}

void
raw_display_tm_player_info(struct qserver *server)
{
	static const char *fmt = "%s" "%s%d" "%s%s" "%s%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
					 xform_name(player->name, server), RD,
					 player->ping, RD,
					 player->skin ? player->skin : "", RD,
					 play_time(player->connect_time, 1)
					 );
		fputs("\n", OF);
	}
}

void
raw_display_tee_player_info(struct qserver *server)
{
	static const char *fmt = "%s";
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server));
		fputs("\n", OF);
	}
}

void
raw_display_ravenshield_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}

void
raw_display_savage_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}

void
raw_display_farcry_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}