/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Inspired by QuakePing by Len Norton
 *
 * Standard output
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002,2003,2004 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "display_standard.h"

void
standard_display_server(struct qserver *server)
{
	char prefix[64];
	if (display_prefix)
	{
		sprintf(prefix, "%-4s ", server->type->type_prefix);
	}
	else
	{
		prefix[0] = '\0';
	}

	if (server->server_name == DOWN || server->server_name == SYSERROR)
	{
		if (!up_servers_only)
		{
			xform_printf(OF, "%s%-16s %10s\n", prefix, (hostname_lookup) ? server->host_name: server->arg, server->server_name);
		}
		return ;
	}

	if (server->server_name == TIMEOUT)
	{
		if (server->flags &FLAG_BROADCAST && server->n_servers)
		{
			xform_printf(OF, "%s%-16s %d servers\n", prefix, server->arg, server->n_servers);
		}
		else if (!up_servers_only)
		{
			xform_printf(OF, "%s%-16s no response\n", prefix, (hostname_lookup) ? server->host_name: server->arg);
		}
		return ;
	}

	if (server->type->master)
	{
		standard_display_qwmaster(server);
		return ;
	}

	if (no_full_servers && server->num_players >= server->max_players)
	{
		return ;
	}

	if (no_empty_servers && server->num_players == 0)
	{
		return ;
	}

	if (server->error != NULL)
	{
		xform_printf(OF, "%s%-21s ERROR <%s>\n", prefix, (hostname_lookup) ? server->host_name: server->arg, server->error);
		return ;
	}

	if (new_style)
	{
		char *game = get_qw_game(server);
		int map_name_width = 8, game_width = 0;
		switch (server->type->id)
		{
			case QW_SERVER:
			case Q2_SERVER:
			case Q3_SERVER:
				game_width = 9;
				break;
			case TRIBES2_SERVER:
				map_name_width = 14;
				game_width = 8;
				break;
			case GHOSTRECON_SERVER:
				map_name_width = 15;
				game_width = 15;
				break;
			case HL_SERVER:
				map_name_width = 12;
				break;
			default:
				break;
		}
		xform_printf(OF, 
			"%s%-21s %2d/%-2d %2d/%-2d %*s %6d / %1d  %*s %s\n",
			prefix,
			(hostname_lookup) ? server->host_name: server->arg,
			server->num_players,
			server->max_players,
			server->num_spectators,
			server->max_spectators,
			map_name_width,
			(server->map_name) ? xform_name(server->map_name, server): "?",
			server->n_requests ? server->ping_total / server->n_requests: 999,
			server->n_retries,
			game_width,
			game,
			xform_name(server->server_name, server)
		);
		if (get_server_rules && NULL != server->type->display_rule_func )
		{
			server->type->display_rule_func(server);
		}
		if (get_player_info && NULL != server->type->display_player_func )
		{
			server->type->display_player_func(server);
		}
	}
	else
	{
		char name[512];
		sprintf(name, "\"%s\"", server->server_name);
		xform_printf(OF, 
			"%-16s %10s map %s at %22s %d/%d players %d ms\n",
			(hostname_lookup) ? server->host_name: server->arg,
			name,
			server->map_name,
			server->address,
			server->num_players,
			server->max_players,
			server->n_requests ? server->ping_total / server->n_requests: 999
		);
	}
}

void
standard_display_qwmaster(struct qserver *server)
{
	char *prefix;
	prefix = server->type->type_prefix;

	if (server->error != NULL)
	{
		xform_printf(OF, 
			"%s %-17s ERROR <%s>\n",
			prefix,
			(hostname_lookup) ? server->host_name: server->arg,
			server->error
		);
	}
	else
	{
		xform_printf(OF, 
			"%s %-17s %d servers %6d / %1d\n",
			prefix,
			(hostname_lookup) ? server->host_name: server->arg,
			server->n_servers,
			server->n_requests ? server->ping_total / server->n_requests: 999,
			server->n_retries
		);
	}
}

void
standard_display_header()
{
	if (!no_header_display)
	{
		xform_printf(OF, "%-16s %8s %8s %15s    %s\n", "ADDRESS", "PLAYERS", "MAP", "RESPONSE TIME", "NAME");
	}
}

void
standard_display_server_rules(struct qserver *server)
{
	struct rule *rule;
	int printed = 0;
	rule = server->rules;
	for (; rule != NULL; rule = rule->next)
	{
		if ((server->type->id != Q_SERVER && server->type->id != H2_SERVER) || !is_default_rule(rule))
		{
			xform_printf(OF, "%c%s=%s", (printed) ? ',' : '\t', rule->name, rule->value);
			printed++;
		}
	}
	if (printed)
	{
		fputs("\n", OF);
	}
}

void
standard_display_q_player_info(struct qserver *server)
{
	char fmt[128];
	struct player *player;

	strcpy(fmt, "\t#%-2d %3d frags %9s ");

	if (color_names)
	{
		strcat(fmt, "%9s:%-9s ");
	}
	else
	{
		strcat(fmt, "%2s:%-2s ");
	}
	if (player_address)
	{
		strcat(fmt, "%22s ");
	}
	else
	{
		strcat(fmt, "%s");
	}
	strcat(fmt, "%s\n");

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, fmt,
			player->number,
			player->frags,
			play_time(player->connect_time, 1),
			quake_color(player->shirt_color),
			quake_color(player->pants_color),
			(player_address) ? player->address: "",
			xform_name(player->name, server)
		);
	}
}

void
standard_display_qw_player_info(struct qserver *server)
{
	char fmt[128];
	struct player *player;

	strcpy(fmt, "\t#%-6d %5d frags %6s@%-5s %8s");

	if (color_names)
	{
		strcat(fmt, "%9s:%-9s ");
	}
	else
	{
		strcat(fmt, "%2s:%-2s ");
	}
	strcat(fmt, "%12s %s\n");

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, fmt,
			player->number,
			player->frags,
			play_time(player->connect_time, 0),
			ping_time(player->ping),
			player->skin ? player->skin: "",
			quake_color(player->shirt_color),
			quake_color(player->pants_color),
			xform_name(player->name, server),
			xform_name(player->team_name, server)
		);
	}
}

void
standard_display_q2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		if (server->flags &FLAG_PLAYER_TEAMS)
		{
			xform_printf(OF, "\t%3d frags team#%d %8s  %s\n", player->frags, player->team, ping_time(player->ping), xform_name(player->name, server));
		}
		else
		{
			xform_printf(OF, "\t%3d frags %8s  %s\n", player->frags, ping_time(player->ping), xform_name(player->name, server));
		}
	}
}



void
standard_display_unreal_player_info(struct qserver *server)
{
	struct player *player;
	static const char *fmt_team_number = "\t%3d frags team#%-3d %7s %s\n";
	static const char *fmt_team_name = "\t%3d frags %8s %7s %s\n";
	static const char *fmt_no_team = "\t%3d frags %8s  %s\n";

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		if (server->flags &FLAG_PLAYER_TEAMS)
		{
			// we use (player->score) ? player->score : player->frags,
			// so we get details from halo
			if (player->team_name)
			{
				xform_printf(OF, fmt_team_name,
					(player->score && NA_INT != player->score) ? player->score: player->frags,
					player->team_name,
					ping_time(player->ping),
					xform_name(player->name, server)
				);
			}
			else
			{
				xform_printf(OF, fmt_team_number,
					(player->score && NA_INT != player->score) ? player->score: player->frags,
					player->team,
					ping_time(player->ping),
					xform_name(player->name, server)
				);
			}
		}
		else
		{
			xform_printf(OF, fmt_no_team, player->frags, ping_time(player->ping), xform_name(player->name, server));
		}
	}
}

void
standard_display_shogo_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, ping_time(player->ping), xform_name(player->name, server));
	}
}

void
standard_display_halflife_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}

void
standard_display_fl_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %8s %s\n", player->frags, ping_time(player->ping), play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}

void
standard_display_tribes_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%4d score team#%d %8s %s\n", player->frags, player->team, ping_time(player->ping), xform_name(player->name, server)
	);
	}
}

void
standard_display_tribes2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\tscore %4d %14s %s\n",
			player->frags,
			player->team_name ? player->team_name: (player->number == TRIBES_TEAM ?	"TEAM" : "?"),
			xform_name(player->name, server)
		);
	}
}

void
standard_display_bfris_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\ttid: %d, ship: %d, team: %s, ping: %d, score: %d, kills: %d, name: %s\n",
			player->number,
			player->ship,
			player->team_name,
			player->ping,
			player->score,
			player->frags,
			xform_name(player->name, server)
		);
	}
}

void
standard_display_descent3_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %3d deaths team#%-3d %7s %s\n",
			player->frags,
			player->deaths,
			player->team,
			ping_time(player->ping),
			xform_name(player->name, server)
		);
	}
}

void
standard_display_ghostrecon_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\tdead=%3d team#%-3d %s\n", player->deaths, player->team, xform_name(player->name, server));
	}
}

void
standard_display_eye_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		if (player->team_name)
		{
			xform_printf(OF, "\tscore %4d %6s team %12s %s\n",
				player->score,
				ping_time(player->ping),
				player->team_name,
				xform_name(player->name,server)
			);
		}
		else
		{
			xform_printf(OF, "\tscore %4d %6s team#%d %s\n",
				player->score,
				ping_time(player->ping),
				player->team,
				xform_name(player->name,server)
			);
		}
	}
}

void
standard_display_gs2_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		if (player->team_name)
		{
			xform_printf(OF, "\tscore %4d %6s team %12s %s\n",
				player->score,
				ping_time(player->ping),
				player->team_name,
				xform_name(player->name,server)
			);
		}
		else
		{
			xform_printf(OF, "\tscore %4d %6s team#%d %s\n",
				player->score,
				ping_time(player->ping),
				player->team,
				xform_name(player->name, server)
			);
		}
	}
}

void
standard_display_armyops_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		player->score = calculate_armyops_score(player);
	}

	standard_display_gs2_player_info(server);
}

void
standard_display_ts2_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%6s %s\n", ping_time(player->ping), xform_name(player->name, server));
	}
}

void
standard_display_ts3_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}

void
standard_display_starmade_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}

void
standard_display_bfbc2_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}

void
standard_display_wic_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t#%-4d score %4d team %12s role %12s %s\n",
			player->number,
			player->score,
			player->team_name,
			player->tribe_tag ? player->tribe_tag : "",
			xform_name(player->name, server)
		);
	}
}

void
standard_display_ventrilo_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t# %d ping %s time %d cid %i ch %s name %s\n",
			player->number,
			ping_time(player->ping),
			player->connect_time,
			player->team,
			player->team_name,
			xform_name(player->name, server)
		);
	}
}

void
standard_display_tm_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%6s %s\n", ping_time(player->ping), xform_name(player->name, server));
	}
}

void
standard_display_doom3_player_info(struct qserver *server)
{
	struct player *player;
	player = server->players;
	for (; player != NULL; player = player->next)
	{
		if (player->tribe_tag)
		{
			xform_printf(OF, "\t#%-4d score %4d %6s team %12s %s\n",
				player->number,
				player->score,
				ping_time(player->ping),
				player->tribe_tag,
				xform_name(player->name, server)
			);
		}
		else
		{
			xform_printf(OF, "\t#%-4d score %4d %6s team#%d %s\n",
				player->number,
				player->score,
				ping_time(player->ping),
				player->team,
				xform_name(player->name, server)
			);
		}
	}
}

void
standard_display_ravenshield_player_info(struct qserver *server)
{
	struct player *player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
standard_display_savage_player_info(struct qserver *server)
{
	struct player *player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
standard_display_farcry_player_info(struct qserver *server)
{
	struct player *player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}

void
standard_display_tee_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next)
	{
		xform_printf(OF, "\t%4d score %s\n", player->score, xform_name(player->name, server));
	}
}
