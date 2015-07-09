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

#include "qstat.h"
#include "qserver.h"
#include "xform.h"

#define MAXSTRLEN 2048

int output_bom;
int xml_display;
int xml_encoding;

void xml_header();
void xml_footer();

char* xml_escape(char *string);

void xml_display_server(struct qserver *server);
void xml_display_server_rules(struct qserver *server);

void xml_display_armyops_player_info(struct qserver *server);
void xml_display_bfbc2_player_info(struct qserver *server);
void xml_display_bfris_player_info(struct qserver *server);
void xml_display_descent3_player_info(struct qserver *server);
void xml_display_doom3_player_info(struct qserver *server);
void xml_display_eye_player_info(struct qserver *server);
void xml_display_farcry_player_info(struct qserver *server);
void xml_display_fl_player_info(struct qserver *server);
void xml_display_ghostrecon_player_info(struct qserver *server);
void xml_display_halflife_player_info(struct qserver *server);
void xml_display_player_info_info(struct player *player);
void xml_display_player_info(struct qserver *server);
void xml_display_q2_player_info(struct qserver *server);
void xml_display_q_player_info(struct qserver *server);
void xml_display_qw_player_info(struct qserver *server);
void xml_display_ravenshield_player_info(struct qserver *server);
void xml_display_savage_player_info(struct qserver *server);
void xml_display_starmade_player_info(struct qserver *server);
void xml_display_tee_player_info(struct qserver *server);
void xml_display_tm_player_info(struct qserver *server);
void xml_display_tribes2_player_info(struct qserver *server);
void xml_display_tribes_player_info(struct qserver *server);
void xml_display_ts2_player_info(struct qserver *server);
void xml_display_ts3_player_info(struct qserver *server);
void xml_display_unreal_player_info(struct qserver *server);
void xml_display_ventrilo_player_info(struct qserver *server);
void xml_display_wic_player_info(struct qserver *server);