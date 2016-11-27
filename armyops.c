/*
 * qstat
 *
 * Armyops Protocol
 * Copyright 2005 Omnix
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 *
 */

#include "qstat.h"

void
json_display_armyops_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for (; player != NULL; player = player->next) {
		player->score = calculate_armyops_score(player);
	}

	json_display_player_info(server);
}

// vim: sw=4 ts=4 noet
