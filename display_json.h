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
#ifndef QSTAT_DISPLAY_JSON_H
#define QSTAT_DISPLAY_JSON_H

#include "qstat.h"
#include "qserver.h"

int json_display;
int json_encoding;
int json_printed;

void json_footer();
void json_header();
void json_protocols();
void json_version();

char* json_escape(char *string);

void json_display_server(struct qserver *server);
void json_display_server_rules(struct qserver *server);

void json_display_bfbc2_player_info(struct qserver *server);
void json_display_bfris_player_info(struct qserver *server);
void json_display_descent3_player_info(struct qserver *server);
void json_display_doom3_player_info(struct qserver *server);
void json_display_eye_player_info(struct qserver *server);
void json_display_farcry_player_info(struct qserver *server);
void json_display_fl_player_info(struct qserver *server);
void json_display_ghostrecon_player_info(struct qserver *server);
void json_display_halflife_player_info(struct qserver *server);
void json_display_player_info_info(struct player *player);
void json_display_player_info(struct qserver *server);
void json_display_q2_player_info(struct qserver *server);
void json_display_q_player_info(struct qserver *server);
void json_display_qw_player_info(struct qserver *server);
void json_display_ravenshield_player_info(struct qserver *server);
void json_display_savage_player_info(struct qserver *server);
void json_display_starmade_player_info(struct qserver *server);
void json_display_tee_player_info(struct qserver *server);
void json_display_tm_player_info(struct qserver *server);
void json_display_tribes2_player_info(struct qserver *server);
void json_display_tribes_player_info(struct qserver *server);
void json_display_ts2_player_info(struct qserver *server);
void json_display_ts3_player_info(struct qserver *server);
void json_display_unreal_player_info(struct qserver *server);
void json_display_ventrilo_player_info(struct qserver *server);
void json_display_wic_player_info(struct qserver *server);

#endif
