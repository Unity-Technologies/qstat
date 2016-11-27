/*
 * qstat
 *
 * Armyops Protocol
 * Copyright 2005 Omnix
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_ARMOPS_H
#define QSTAT_ARMOPS_H

int calculate_armyops_score(struct player *player);
void json_display_armyops_player_info(struct qserver *server);

#endif
