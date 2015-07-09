/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Inspired by QuakePing by Len Norton
 *
 * XML output
 * Contributed by Simon Garner <sgarner@gameplanet.co.nz>
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002,2003,2004 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include "display_xml.h"

int output_bom = 0;
int xml_display = 0;
int xml_encoding = ENCODING_LATIN_1;

void
xml_display_server(struct qserver *server)
{
	char *prefix;
	prefix = server->type->type_prefix;

	if (server->server_name == DOWN) {
		if (!up_servers_only) {
			xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\">\n", xml_escape(prefix), xml_escape(server->arg), xml_escape(DOWN));
			xform_printf(OF, "\t\t<hostname>%s</hostname>\n", xml_escape((hostname_lookup) ? server->host_name : server->arg));
			xform_printf(OF, "\t</server>\n");
		}
		return;
	}
	if (server->server_name == TIMEOUT) {
		if (server->flags & FLAG_BROADCAST && server->n_servers) {
			xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\" servers=\"%d\">\n",
						 xml_escape(prefix),
						 xml_escape(server->arg),
						 xml_escape(TIMEOUT),
						 server->n_servers
						 );
			xform_printf(OF, "\t</server>\n");
		} else if (!up_servers_only)   {
			xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\">\n", xml_escape(prefix), xml_escape(server->arg), xml_escape(TIMEOUT));
			xform_printf(OF, "\t\t<hostname>%s</hostname>\n", xml_escape((hostname_lookup) ? server->host_name : server->arg));
			xform_printf(OF, "\t</server>\n");
		}
		return;
	}

	if (server->error != NULL) {
		xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\">\n", xml_escape(prefix), xml_escape(server->arg), "ERROR");
		xform_printf(OF, "\t\t<hostname>%s</hostname>\n", xml_escape((hostname_lookup) ? server->host_name : server->arg));
		xform_printf(OF, "\t\t<error>%s</error>\n", xml_escape(server->error));
	} else if (server->type->master)   {
		xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\" servers=\"%d\">\n", xml_escape(prefix), xml_escape(server->arg), "UP", server->n_servers);
	} else   {
		xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\">\n", xml_escape(prefix), xml_escape(server->arg), "UP");
		xform_printf(OF, "\t\t<hostname>%s</hostname>\n", xml_escape((hostname_lookup) ? server->host_name : server->arg));
		xform_printf(OF, "\t\t<name>%s</name>\n", xml_escape(xform_name(server->server_name, server)));
		xform_printf(OF, "\t\t<gametype>%s</gametype>\n", xml_escape(get_qw_game(server)));
		xform_printf(OF, "\t\t<map>%s</map>\n", xml_escape(xform_name(server->map_name, server)));
		xform_printf(OF, "\t\t<numplayers>%d</numplayers>\n", server->num_players);
		xform_printf(OF, "\t\t<maxplayers>%d</maxplayers>\n", server->max_players);
		xform_printf(OF, "\t\t<numspectators>%d</numspectators>\n", server->num_spectators);
		xform_printf(OF, "\t\t<maxspectators>%d</maxspectators>\n", server->max_spectators);

		if (!(server->type->flags & TF_RAW_STYLE_TRIBES)) {
			xform_printf(OF, "\t\t<ping>%d</ping>\n", server->n_requests ? server->ping_total / server->n_requests : 999);
			xform_printf(OF, "\t\t<retries>%d</retries>\n", server->n_retries);
		}

		if (server->type->flags & TF_RAW_STYLE_QUAKE) {
			xform_printf(OF, "\t\t<address>%s</address>\n", xml_escape(server->address));
			xform_printf(OF, "\t\t<protocolversion>%d</protocolversion>\n", server->protocol_version);
		}
	}

	if (!server->type->master && server->error == NULL) {
		if (get_server_rules && NULL != server->type->display_xml_rule_func) {
			server->type->display_xml_rule_func(server);
		}
		if (get_player_info && NULL != server->type->display_xml_player_func) {
			server->type->display_xml_player_func(server);
		}
	}

	xform_printf(OF, "\t</server>\n");
}

void
xml_header()
{
	if (xml_encoding == ENCODING_LATIN_1) {
		xform_printf(OF, "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n<qstat>\n");
	} else if (output_bom)   {
		xform_printf(OF, "%c%c%c<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<qstat>\n", 0xEF, 0xBB, 0xBF);
	} else   {
		xform_printf(OF, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<qstat>\n");
	}
}

void
xml_footer()
{
	xform_printf(OF, "</qstat>\n");
}

void
xml_display_server_rules(struct qserver *server)
{
	struct rule *rule;

	rule = server->rules;

	xform_printf(OF, "\t\t<rules>\n");
	for (; rule != NULL; rule = rule->next) {
		xform_printf(OF, "\t\t\t<rule name=\"%s\">%s</rule>\n", xml_escape(rule->name), xml_escape(rule->value));
	}
	xform_printf(OF, "\t\t</rules>\n");
}

void
xml_display_q_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player number=\"%d\">\n", player->number);

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<address>%s</address>\n", xml_escape(player->address));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		if (color_names) {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", xml_escape(quake_color(player->shirt_color)));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", xml_escape(quake_color(player->pants_color)));
		} else   {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", quake_color(player->shirt_color));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", quake_color(player->pants_color));
		}

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_qw_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player number=\"%d\">\n", player->number);

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		if (color_names) {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", xml_escape(quake_color(player->shirt_color)));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", xml_escape(quake_color(player->pants_color)));
		} else   {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", quake_color(player->shirt_color));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", quake_color(player->pants_color));
		}

		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", player->skin ? xml_escape(player->skin) : "");
		xform_printf(OF, "\t\t\t\t<team>%s</team>\n", player->team_name ? xml_escape(player->team_name) : "");

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_q2_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		if (server->flags & FLAG_PLAYER_TEAMS) {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_player_info_info(struct player *player)
{
	struct info *info;

	for (info = player->info; info; info = info->next) {
		if (info->name) {
			char *name = xml_escape(info->name);
			char *value = xml_escape(info->value);
			xform_printf(OF, "\t\t\t\t<%s>%s</%s>\n", name, value, name);
		}
	}
}

void
xml_display_unreal_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		if (-999 != player->deaths) {
			xform_printf(OF, "\t\t\t\t<deaths>%d</deaths>\n", player->deaths);
		}
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);

		if (player->team_name) {
			xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		} else if (-1 != player->team)   {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}

		// Some games dont provide
		// so only display if they do
		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", player->skin ? xml_escape(player->skin) : "");
		}
		if (player->mesh) {
			xform_printf(OF, "\t\t\t\t<mesh>%s</mesh>\n", player->mesh ? xml_escape(player->mesh) : "");
		}
		if (player->face) {
			xform_printf(OF, "\t\t\t\t<face>%s</face>\n", player->face ? xml_escape(player->face) : "");
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_halflife_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_fl_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_tribes_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<packetloss>%d</packetloss>\n", player->packet_loss);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_tribes2_player_info(struct qserver *server)
{
	struct player *player;

	char *type;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		if (player->team_name) {
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

			xform_printf(OF, "\t\t\t<player>\n");

			xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
			xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
			xform_printf(OF, "\t\t\t\t<team number=\"%d\">%s</team>\n", player->team, xml_escape(player->team_name));
			xform_printf(OF, "\t\t\t\t<type>%s</type>\n", xml_escape(type));
			xform_printf(OF, "\t\t\t\t<clan>%s</clan>\n", player->tribe_tag ? xml_escape(xform_name(player->tribe_tag, server)) : "");

			xform_printf(OF, "\t\t\t</player>\n");
		}
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_bfris_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player number=\"%d\">\n", player->number);

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score type=\"score\">%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<score type=\"frags\">%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<ship>%d</ship>\n", player->ship);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_descent3_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<deaths>%d</deaths>\n", player->deaths);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_ravenshield_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_ghostrecon_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<deaths>%d</deaths>\n", player->deaths);
		xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_eye_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		if (player->team_name) {
			xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		} else   {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}
		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", xml_escape(player->skin));
		}
		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 1)));
		}

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_doom3_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<number>%u</number>\n", player->number);
		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		if (player->tribe_tag) {
			xform_printf(OF, "\t\t\t\t<clan>%s</clan>\n", player->tribe_tag ? xml_escape(xform_name(player->tribe_tag, server)) : "");
		} else   {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}
		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", xml_escape(player->skin));
		}
		if (player->type_flag) {
			xform_printf(OF, "\t\t\t\t<type>bot</type>\n");
		} else   {
			xform_printf(OF, "\t\t\t\t<type>player</type>\n");
		}

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		if (NA_INT != player->ping) {
			xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		}
		if (NA_INT != player->score) {
			xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		}
		if (NA_INT != player->deaths) {
			xform_printf(OF, "\t\t\t\t<deaths>%d</deaths>\n", player->deaths);
		}
		if (NA_INT != player->frags) {
			xform_printf(OF, "\t\t\t\t<frags>%d</frags>\n", player->frags);
		}
		if (player->team_name) {
			xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		} else if (NA_INT != player->team)   {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}

		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", xml_escape(player->skin));
		}

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 1)));
		}

		xml_display_player_info_info(player);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_armyops_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		player->score = calculate_armyops_score(player);
	}

	xml_display_player_info(server);
}

void
xml_display_ts2_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_ts3_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_starmade_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_bfbc2_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_wic_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<team>%s</team>\n", player->team_name);
		xform_printf(OF, "\t\t\t\t<bot>%d</bot>\n", player->type_flag);
		if (player->tribe_tag) {
			xform_printf(OF, "\t\t\t\t<role>%s</role>\n", player->tribe_tag);
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_ventrilo_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_tm_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));
		}

		xml_display_player_info_info(player);
		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_savage_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_farcry_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%su</time>\n", xml_escape(play_time(player->connect_time, 2)));

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

void
xml_display_tee_player_info(struct qserver *server)
{
	struct player *player;

	xform_printf(OF, "\t\t<players>\n");

	player = server->players;
	for (; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}

char
*
xml_escape(char *string)
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
		case '&':
			*b++ = '&';
			*b++ = 'a';
			*b++ = 'm';
			*b++ = 'p';
			*b++ = ';';
			continue;
		case '\'':
			*b++ = '&';
			*b++ = 'a';
			*b++ = 'p';
			*b++ = 'o';
			*b++ = 's';
			*b++ = ';';
			continue;
		case '"':
			*b++ = '&';
			*b++ = 'q';
			*b++ = 'u';
			*b++ = 'o';
			*b++ = 't';
			*b++ = ';';
			continue;
		case '<':
			*b++ = '&';
			*b++ = 'l';
			*b++ = 't';
			*b++ = ';';
			continue;
		case '>':
			*b++ = '&';
			*b++ = 'g';
			*b++ = 't';
			*b++ = ';';
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
		} else if (xml_encoding == ENCODING_LATIN_1)   {
			if (!xform_names) {
				*b++ = c;
			} else   {
				if (isprint(c)) {
					*b++ = c;
				} else   {
					b += sprintf((char *)b, "&#%u;", c);
				}
			}
		} else if (xml_encoding == ENCODING_UTF_8)   {
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