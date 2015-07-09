/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Inspired by QuakePing by Len Norton
 *
 * Template output
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#if defined(__hpux) || defined(_AIX) || defined(__FreeBSD__) || defined(__MidnightBSD__)
	#include <sys/types.h>
#endif

#ifndef _WIN32
	#include <netinet/in.h>
#endif

#include "qstat.h"
#include "xform.h"

#ifdef _WIN32
	#define strcasecmp  stricmp
	#define strncasecmp strnicmp
#endif

#ifdef __hpux
	#define STATIC static
#else
	#define STATIC
#endif

/*
   #ifdef __cplusplus
   extern "C" {
   #endif
   #ifndef _AIX
   extern unsigned int ntohl(unsigned int n);
   #endif
   #ifdef __cplusplus
   }
   #endif
 */

int have_header_template();
int have_trailer_template();
int have_server_template();

int read_header_template(char *filename);
int read_trailer_template(char *filename);

int read_qserver_template(char *filename);
int read_player_template(char *filename);
int read_rule_template(char *filename);

void template_display_header();
void template_display_trailer();

void template_display_server(struct qserver *server);

void template_display_players(struct qserver *server);
void template_display_player(struct qserver *server, struct player *player);

void template_display_rules(struct qserver *server);
void template_display_rule(struct qserver *server, struct rule *rule);