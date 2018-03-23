/*
 * qstat
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Thanks to Per Hammer for the OS/2 patches (per@mindbend.demon.co.uk)
 * Thanks to John Ross Hunt for the OpenVMS Alpha patches (bigboote@ais.net)
 * Thanks to Scott MacFiggen for the quicksort code (smf@webmethods.com)
 * Thanks to Simon Garner for the XML patch (sgarner@gameplanet.co.nz)
 * Thanks to Bob Marriott for the Ghost Recon code (bmarriott@speakeasy.net)
 *
 * Inspired by QuakePing by Len Norton
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002,2003,2004 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#define RECV_BUF    204800

/* OS/2 defines */
#ifdef __OS2__
	#define BSD_SELECT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#define QUERY_PACKETS
#include "qstat.h"
#include "packet_manip.h"
#include "config.h"
#include "xform.h"

#ifndef _WIN32
 #include <signal.h>
 #include <unistd.h>
 #include <sys/socket.h>
 #ifndef VMS
  #include <sys/param.h>
 #endif
 #include <sys/time.h>
 #include <netinet/in.h>
 #include <netinet/tcp.h>
 #include <netdb.h>
 #include <arpa/inet.h>

 #ifndef F_SETFL
  #include <fcntl.h>
 #endif

 #ifdef __hpux
		extern int h_errno;
		#define STATIC    static
 #else
		#define STATIC
 #endif

	#define INVALID_SOCKET		-1
 #ifndef INADDR_NONE
		#define INADDR_NONE	~0
 #endif
	#define sockerr()    errno
#else   /* _WIN32 */
 #include "utils.h"
#endif  /* !_WIN32 */

#ifdef __OS2__
 #include <sys/socket.h>
 #include <sys/select.h>
 #include <sys/time.h>
 #include <netinet/in.h>
 #include <netdb.h>
 #include <utils.h>

	#define INVALID_SOCKET		-1
	#define SOCKET_ERROR		-1
	#define close(a)    soclose(a)
#endif  /* __OS2__ */

#ifndef FD_SETSIZE
	#define FD_SETSIZE    64
#endif

/* Figure out whether to use poll() or select()
 */
#ifndef USE_POLL
 #ifndef USE_SELECT

  #ifdef sun
			#define USE_POLL
  #endif
  #ifdef linux
			#define USE_POLL
   #include <poll.h>
  #endif
  #ifdef __linux__
			#define USE_POLL
   #include <poll.h>
  #endif
  #ifdef __linux
			#define USE_POLL
   #include <poll.h>
  #endif
  #ifdef __hpux
			#define USE_POLL
   #include <sys/poll.h>
  #endif
  #ifdef __OpenBSD__
			#define USE_POLL
   #include <sys/poll.h>
  #endif
  #ifdef _AIX
			#define USE_POLL
   #include <poll.h>
  #endif
  #ifdef _WIN32
			#define USE_SELECT
  #endif
  #ifdef __EMX__
			#define USE_SELECT
  #endif

 #endif         /* USE_SELECT */
#endif  /* USE_POLL */

/* If did not chose, then use select()
 */
#ifndef USE_POLL
 #ifndef USE_SELECT
		#define USE_SELECT
 #endif
#endif

#include "debug.h"

server_type *types;
int n_server_types;
char *qstat_version = VERSION;

/*
 * Values set by command-line arguments
 */

int hostname_lookup = 0;        /* set if -H was specified */
int new_style = 1;              /* unset if -old was specified */
int n_retries = DEFAULT_RETRIES;
int retry_interval = DEFAULT_RETRY_INTERVAL;
int master_retry_interval = DEFAULT_RETRY_INTERVAL * 4;
int syncconnect = 0;
int get_player_info = 0;
int get_server_rules = 0;
int up_servers_only = 0;
int no_full_servers = 0;
int no_empty_servers = 0;
int no_header_display = 0;
int raw_display = 0;
char *raw_delimiter = "\t";
char *multi_delimiter = "|";
int player_address = 0;
int max_simultaneous = MAXFD_DEFAULT;
int sendinterval = 5;
extern int xform_names;
extern int xform_strip_unprintable;
extern int xform_hex_player_names;
extern int xform_hex_server_names;
extern int xform_strip_carets;
extern int xform_html_names;
extern int html_mode;
int raw_arg = 0;
int show_game_in_raw = 0;
int progress = 0;
int num_servers_total = 0;
int num_players_total = 0;
int max_players_total = 0;
int num_servers_returned = 0;
int num_servers_timed_out = 0;
int num_servers_down = 0;
server_type *default_server_type = NULL;
FILE *OF;       /* output file */
unsigned int source_ip = INADDR_ANY;
unsigned short source_port_low = 0;
unsigned short source_port_high = 0;
unsigned short source_port = 0;
int show_game_port = 0;
int no_port_offset = 0;

int output_bom = 0;
int xml_display = 0;
int xml_encoding = ENCODING_LATIN_1;

#define SUPPORTED_SERVER_SORT		"pgihn"
#define SUPPORTED_PLAYER_SORT		"PFTNS"
#define SUPPORTED_SORT_KEYS		"l" SUPPORTED_SERVER_SORT SUPPORTED_PLAYER_SORT
char sort_keys[32];
int player_sort = 0;
int server_sort = 0;

int qpartition(void **array, int i, int j, int (*compare)(void *, void *));
void sort_servers(struct qserver **array, int size);
void sort_players(struct qserver *server);
int server_compare(struct qserver *one, struct qserver *two);
int player_compare(struct player *one, struct player *two);
int process_func_ret(struct qserver *server, int ret);
int connection_inprogress();
void clear_socketerror();

int show_errors = 0;
static int noserverdups = 1;

#define DEFAULT_COLOR_NAMES_RAW		0
#define DEFAULT_COLOR_NAMES_DISPLAY	1
int color_names = -1;

#define SECONDS				0
#define CLOCK_TIME			1
#define STOPWATCH_TIME			2
#define DEFAULT_TIME_FMT_RAW		SECONDS
#define DEFAULT_TIME_FMT_DISPLAY	CLOCK_TIME
int time_format = -1;

struct qserver *servers = NULL;
struct qserver **last_server = &servers;
struct qserver **connmap = NULL;
int max_connmap;
struct qserver *last_server_bind = NULL;
struct qserver *first_server_bind = NULL;
int connected = 0;
time_t run_timeout = 0;
time_t start_time;
int waiting_for_masters;

#define ADDRESS_HASH_LENGTH    2999
static unsigned num_servers;/* current number of servers in memory */
static struct qserver **server_hash[ADDRESS_HASH_LENGTH];
static unsigned int server_hash_len[ADDRESS_HASH_LENGTH];
static void free_server_hash();
static void xml_display_player_info_info(struct player *player);

char *DOWN = "DOWN";
char *SYSERROR = "SYSERROR";
char *TIMEOUT = "TIMEOUT";
char *MASTER = "MASTER";
char *SERVERERROR = "ERROR";
char *HOSTNOTFOUND = "HOSTNOTFOUND";
char *BFRIS_SERVER_NAME = "BFRIS Server";
char *GAMESPY_MASTER_NAME = "Gamespy Master";

int display_prefix = 0;
char *current_filename;
int current_fileline;

int count_bits(int n);

static int qserver_get_timeout(struct qserver *server, struct timeval *now);
static int wait_for_timeout(unsigned int ms);
static void finish_output();
static int decode_stefmaster_packet(struct qserver *server, char *pkt, int pktlen);
static int decode_q3master_packet(struct qserver *server, char *ikt, int pktlen);

char *ut2003_strdup(const char *string, const char *end, char **next);

void free_server(struct qserver *server);
void free_player(struct player *player);
void free_rule(struct rule *rule);
void standard_display_server(struct qserver *server);

/* MODIFY HERE
 * Change these functions to display however you want
 */
void
display_server(struct qserver *server)
{
	if (player_sort) {
		sort_players(server);
	}

	if (raw_display) {
		raw_display_server(server);
	} else if (xml_display) {
		xml_display_server(server);
	} else if (json_display) {
		json_display_server(server);
	} else if (have_server_template()) {
		template_display_server(server);
	} else {
		standard_display_server(server);
	}

	free_server(server);
}


void
standard_display_server(struct qserver *server)
{
	char prefix[64];

	if (display_prefix) {
		sprintf(prefix, "%-4s ", server->type->type_prefix);
	} else {
		prefix[0] = '\0';
	}

	if ((server->server_name == DOWN) || (server->server_name == SYSERROR)) {
		if (!up_servers_only) {
			xform_printf(OF, "%s%-16s %10s\n", prefix, (hostname_lookup) ? server->host_name : server->arg, server->server_name);
		}
		return;
	}

	if (server->server_name == TIMEOUT) {
		if (server->flags & FLAG_BROADCAST && server->n_servers) {
			xform_printf(OF, "%s%-16s %d servers\n", prefix, server->arg, server->n_servers);
		} else if (!up_servers_only) {
			xform_printf(OF, "%s%-16s no response\n", prefix, (hostname_lookup) ? server->host_name : server->arg);
		}
		return;
	}

	if (server->type->master) {
		display_qwmaster(server);
		return;
	}

	if (no_full_servers && (server->num_players >= server->max_players)) {
		return;
	}

	if (no_empty_servers && (server->num_players == 0)) {
		return;
	}

	if (server->error != NULL) {
		xform_printf(OF, "%s%-21s ERROR <%s>\n", prefix, (hostname_lookup) ? server->host_name : server->arg, server->error);
		return;
	}

	if (new_style) {
		char *game = get_qw_game(server);
		int map_name_width = 8, game_width = 0;
		switch (server->type->id) {
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
		    (hostname_lookup) ? server->host_name : server->arg,
		    server->num_players,
		    server->max_players,
		    server->num_spectators,
		    server->max_spectators,
		    map_name_width,
		    (server->map_name) ? xform_name(server->map_name, server) : "?",
		    server->n_requests ? server->ping_total / server->n_requests : 999,
		    server->n_retries,
		    game_width,
		    game,
		    xform_name(server->server_name, server)
		    );
		if (get_server_rules && (NULL != server->type->display_rule_func)) {
			server->type->display_rule_func(server);
		}
		if (get_player_info && (NULL != server->type->display_player_func)) {
			server->type->display_player_func(server);
		}
	} else {
		char name[512];
		sprintf(name, "\"%s\"", server->server_name);
		xform_printf(OF,
		    "%-16s %10s map %s at %22s %d/%d players %d ms\n",
		    (hostname_lookup) ? server->host_name : server->arg,
		    name,
		    server->map_name,
		    server->address,
		    server->num_players,
		    server->max_players,
		    server->n_requests ? server->ping_total / server->n_requests : 999
		    );
	}
}


void
display_qwmaster(struct qserver *server)
{
	char *prefix;

	prefix = server->type->type_prefix;

	if (server->error != NULL) {
		xform_printf(OF,
		    "%s %-17s ERROR <%s>\n",
		    prefix,
		    (hostname_lookup) ? server->host_name : server->arg,
		    server->error
		    );
	} else {
		xform_printf(OF,
		    "%s %-17s %d servers %6d / %1d\n",
		    prefix,
		    (hostname_lookup) ? server->host_name : server->arg,
		    server->n_servers,
		    server->n_requests ? server->ping_total / server->n_requests : 999,
		    server->n_retries
		    );
	}
}


void
display_header()
{
	if (!no_header_display) {
		xform_printf(OF, "%-16s %8s %8s %15s    %s\n", "ADDRESS", "PLAYERS", "MAP", "RESPONSE TIME", "NAME");
	}
}


void
display_server_rules(struct qserver *server)
{
	struct rule *rule;
	int printed = 0;

	rule = server->rules;
	for ( ; rule != NULL; rule = rule->next) {
		if (((server->type->id != Q_SERVER) && (server->type->id != H2_SERVER)) || !is_default_rule(rule)) {
			xform_printf(OF, "%c%s=%s", (printed) ? ',' : '\t', rule->name, rule->value);
			printed++;
		}
	}
	if (printed) {
		fputs("\n", OF);
	}
}


void
display_q_player_info(struct qserver *server)
{
	char fmt[128];
	struct player *player;

	strcpy(fmt, "\t#%-2d %3d frags %9s ");

	if (color_names) {
		strcat(fmt, "%9s:%-9s ");
	} else {
		strcat(fmt, "%2s:%-2s ");
	}
	if (player_address) {
		strcat(fmt, "%22s ");
	} else {
		strcat(fmt, "%s");
	}
	strcat(fmt, "%s\n");

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
		    player->number,
		    player->frags,
		    play_time(player->connect_time, 1),
		    quake_color(player->shirt_color),
		    quake_color(player->pants_color),
		    (player_address) ? player->address : "",
		    xform_name(player->name, server)
		    );
	}
}


void
display_qw_player_info(struct qserver *server)
{
	char fmt[128];
	struct player *player;

	strcpy(fmt, "\t#%-6d %5d frags %6s@%-5s %8s");

	if (color_names) {
		strcat(fmt, "%9s:%-9s ");
	} else {
		strcat(fmt, "%2s:%-2s ");
	}
	strcat(fmt, "%12s %s\n");

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt,
		    player->number,
		    player->frags,
		    play_time(player->connect_time, 0),
		    ping_time(player->ping),
		    player->skin ? player->skin : "",
		    quake_color(player->shirt_color),
		    quake_color(player->pants_color),
		    xform_name(player->name, server),
		    xform_name(player->team_name, server)
		    );
	}
}


void
display_q2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		if (server->flags & FLAG_PLAYER_TEAMS) {
			xform_printf(OF, "\t%3d frags team#%d %8s  %s\n", player->frags, player->team, ping_time(player->ping), xform_name(player->name, server));
		} else {
			xform_printf(OF, "\t%3d frags %8s  %s\n", player->frags, ping_time(player->ping), xform_name(player->name, server));
		}
	}
}


void
display_unreal_player_info(struct qserver *server)
{
	struct player *player;
	static const char *fmt_team_number = "\t%3d frags team#%-3d %7s %s\n";
	static const char *fmt_team_name = "\t%3d frags %8s %7s %s\n";
	static const char *fmt_no_team = "\t%3d frags %8s  %s\n";

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		if (server->flags & FLAG_PLAYER_TEAMS) {
			// we use (player->score) ? player->score : player->frags,
			// so we get details from halo
			if (player->team_name) {
				xform_printf(OF, fmt_team_name,
				    (player->score && NA_INT != player->score) ? player->score : player->frags,
				    player->team_name,
				    ping_time(player->ping),
				    xform_name(player->name, server)
				    );
			} else {
				xform_printf(OF, fmt_team_number,
				    (player->score && NA_INT != player->score) ? player->score : player->frags,
				    player->team,
				    ping_time(player->ping),
				    xform_name(player->name, server)
				    );
			}
		} else {
			xform_printf(OF, fmt_no_team, player->frags, ping_time(player->ping), xform_name(player->name, server));
		}
	}
}


void
display_shogo_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, ping_time(player->ping), xform_name(player->name, server));
	}
}


void
display_halflife_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
display_fl_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %8s %s\n", player->frags, ping_time(player->ping), play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
display_tribes_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%4d score team#%d %8s %s\n", player->frags, player->team, ping_time(player->ping), xform_name(player->name, server)
		    );
	}
}


void
display_tribes2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\tscore %4d %14s %s\n",
		    player->frags,
		    player->team_name ? player->team_name : (player->number == TRIBES_TEAM ? "TEAM" : "?"),
		    xform_name(player->name, server)
		    );
	}
}


void
display_bfris_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
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
display_descent3_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
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
display_ghostrecon_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\tdead=%3d team#%-3d %s\n", player->deaths, player->team, xform_name(player->name, server));
	}
}


void
display_eye_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, "\tscore %4d %6s team %12s %s\n",
			    player->score,
			    ping_time(player->ping),
			    player->team_name,
			    xform_name(player->name, server)
			    );
		} else {
			xform_printf(OF, "\tscore %4d %6s team#%d %s\n",
			    player->score,
			    ping_time(player->ping),
			    player->team,
			    xform_name(player->name, server)
			    );
		}
	}
}


int
calculate_armyops_score(struct player *player)
{
	/* Calculates a player's score for ArmyOps from the basic components */

	int score = 0;
	int kill_score = 0;
	struct info *info;

	for (info = player->info; info; info = info->next) {
		if ((0 == strcmp(info->name, "leader")) || (0 == strcmp(info->name, "goal")) || (0 == strcmp(info->name, "roe"))) {
			score += atoi(info->value);
		} else if ((0 == strcmp(info->name, "kia")) || (0 == strcmp(info->name, "enemy"))) {
			kill_score += atoi(info->value);
		}
	}

	if (kill_score > 0) {
		score += kill_score;
	}

	return (score);
}


void
display_gs2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, "\tscore %4d %6s team %12s %s\n",
			    player->score,
			    ping_time(player->ping),
			    player->team_name,
			    xform_name(player->name, server)
			    );
		} else {
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
display_armyops_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		player->score = calculate_armyops_score(player);
	}

	display_gs2_player_info(server);
}


void
display_ts2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%6s %s\n", ping_time(player->ping), xform_name(player->name, server));
	}
}


void
display_ts3_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}


void
display_starmade_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}


void
display_bfbc2_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%s\n", xform_name(player->name, server));
	}
}


void
display_wic_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
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
display_ventrilo_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
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
display_tm_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%6s %s\n", ping_time(player->ping), xform_name(player->name, server));
	}
}


void
display_doom3_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		if (player->tribe_tag) {
			xform_printf(OF, "\t#%-4d score %4d %6s team %12s %s\n",
			    player->number,
			    player->score,
			    ping_time(player->ping),
			    player->tribe_tag,
			    xform_name(player->name, server)
			    );
		} else {
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
display_ravenshield_player_info(struct qserver *server)
{
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
display_savage_player_info(struct qserver *server)
{
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
display_farcry_player_info(struct qserver *server)
{
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%3d frags %8s %s\n", player->frags, play_time(player->connect_time, 1), xform_name(player->name, server));
	}
}


void
display_tee_player_info(struct qserver *server)
{
	struct player *player;

	player = server->players;
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t%4d score %s\n", player->score, xform_name(player->name, server));
	}
}


char *
get_qw_game(struct qserver *server)
{
	struct rule *rule;
	char *game_rule = server->type->game_rule;

	if ((game_rule == NULL) || (*game_rule == '\0')) {
		return ("");
	}
	rule = server->rules;
	for ( ; rule != NULL; rule = rule->next) {
		if (strcmp(rule->name, game_rule) == 0) {
			if ((server->type->id == Q3_SERVER) && (strcmp(rule->value, "baseq3") == 0)) {
				return ("");
			}
			return (rule->value);
		}
	}
	rule = server->rules;
	for ( ; rule != NULL; rule = rule->next) {
		if ((0 == strcmp(rule->name, "game")) || (0 == strcmp(rule->name, "fs_game"))) {
			return (rule->value);
		}
	}
	return ("");
}


/* Raw output for web master types
 */

#define RD    raw_delimiter

void
raw_display_server(struct qserver *server)
{
	char *prefix;
	int ping_time;

	prefix = server->type->type_prefix;

	if (server->n_requests) {
		ping_time = server->ping_total / server->n_requests;
	} else {
		ping_time = 999;
	}

	if ((server->server_name == DOWN) || (server->server_name == SYSERROR)) {
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
		} else if (!up_servers_only) {
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
	} else if (server->type->flags & TF_RAW_STYLE_QUAKE) {
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
	} else if (server->type->flags & TF_RAW_STYLE_TRIBES) {
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
	} else if (server->type->flags & TF_RAW_STYLE_GHOSTRECON) {
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
	} else if (server->type->master) {
		xform_printf(OF, "%s" "%.*s%.*s" "%s%s" "%s%d",
		    prefix,
		    raw_arg, RD,
		    raw_arg,
		    server->arg, RD,
		    (hostname_lookup) ? server->host_name : server->arg, RD,
		    server->n_servers
		    );
	} else {
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

	if (server->type->master || (server->error != NULL)) {
		fputs("\n", OF);
		return;
	}

	if (get_server_rules && (NULL != server->type->display_raw_rule_func)) {
		server->type->display_raw_rule_func(server);
	}
	if (get_player_info && (NULL != server->type->display_raw_player_func)) {
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
	for ( ; rule != NULL; rule = rule->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		if (server->flags & FLAG_PLAYER_TEAMS) {
			xform_printf(OF, fmt_team, xform_name(player->name, server), RD, player->frags, RD, player->ping, RD, player->team);
		} else {
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
	for ( ; player != NULL; player = player->next) {
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
		} else {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, fmt_team_name,
			    xform_name(player->name, server), RD,
			    player->score, RD,
			    player->ping, RD,
			    player->team_name, RD,
			    player->skin ? player->skin : "", RD,
			    play_time(player->connect_time, 1)
			    );
		} else {
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
	for ( ; player != NULL; player = player->next) {
		if (player->tribe_tag) {
			xform_printf(OF, fmt_team_name, xform_name(player->name, server), RD, player->score, RD, player->ping, RD, player->tribe_tag, RD, player->number);
		} else {
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
	for ( ; player != NULL; player = player->next) {
		if (player->team_name) {
			xform_printf(OF, fmt_team_name, xform_name(player->name, server), RD,
			    player->score, RD,
			    player->ping, RD,
			    player->team_name, RD,
			    player->skin ? player->skin : "", RD,
			    play_time(player->connect_time, 1)
			    );
		} else {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server));
		fputs("\n", OF);
	}
}


void
raw_display_ravenshield_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}


void
raw_display_savage_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}


void
raw_display_farcry_player_info(struct qserver *server)
{
	static char fmt[24] = "%s" "%s%d" "%s%s";
	struct player *player = server->players;

	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, fmt, xform_name(player->name, server), RD, player->frags, RD, play_time(player->connect_time, 1));
		fputs("\n", OF);
	}
}


/* XML output
 * Contributed by <sgarner@gameplanet.co.nz> :-)
 */
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
		} else if (!up_servers_only) {
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
	} else if (server->type->master) {
		xform_printf(OF, "\t<server type=\"%s\" address=\"%s\" status=\"%s\" servers=\"%d\">\n", xml_escape(prefix), xml_escape(server->arg), "UP", server->n_servers);
	} else {
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

	if (!server->type->master && (server->error == NULL)) {
		if (get_server_rules && (NULL != server->type->display_xml_rule_func)) {
			server->type->display_xml_rule_func(server);
		}
		if (get_player_info && (NULL != server->type->display_xml_player_func)) {
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
	} else if (output_bom) {
		xform_printf(OF, "%c%c%c<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<qstat>\n", 0xEF, 0xBB, 0xBF);
	} else {
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
	for ( ; rule != NULL; rule = rule->next) {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player number=\"%d\">\n", player->number);

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<address>%s</address>\n", xml_escape(player->address));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		if (color_names) {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", xml_escape(quake_color(player->shirt_color)));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", xml_escape(quake_color(player->pants_color)));
		} else {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player number=\"%d\">\n", player->number);

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 2)));

		if (color_names) {
			xform_printf(OF, "\t\t\t\t<color for=\"shirt\">%s</color>\n", xml_escape(quake_color(player->shirt_color)));
			xform_printf(OF, "\t\t\t\t<color for=\"pants\">%s</color>\n", xml_escape(quake_color(player->pants_color)));
		} else {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->frags);
		if (-999 != player->deaths) {
			xform_printf(OF, "\t\t\t\t<deaths>%d</deaths>\n", player->deaths);
		}
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);

		if (player->team_name) {
			xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		} else if (-1 != player->team) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		if (player->team_name) {
			xform_printf(OF, "\t\t\t\t<team>%s</team>\n", xml_escape(player->team_name));
		} else {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<number>%u</number>\n", player->number);
		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);
		xform_printf(OF, "\t\t\t\t<ping>%d</ping>\n", player->ping);
		if (player->tribe_tag) {
			xform_printf(OF, "\t\t\t\t<clan>%s</clan>\n", player->tribe_tag ? xml_escape(xform_name(player->tribe_tag, server)) : "");
		} else {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}
		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", xml_escape(player->skin));
		}
		if (player->type_flag) {
			xform_printf(OF, "\t\t\t\t<type>bot</type>\n");
		} else {
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
	for ( ; player != NULL; player = player->next) {
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
		} else if (NA_INT != player->team) {
			xform_printf(OF, "\t\t\t\t<team>%d</team>\n", player->team);
		}

		if (player->skin) {
			xform_printf(OF, "\t\t\t\t<skin>%s</skin>\n", xml_escape(player->skin));
		}

		if (player->connect_time) {
			xform_printf(OF, "\t\t\t\t<time>%s</time>\n", xml_escape(play_time(player->connect_time, 1)));
		}

		if (player->address) {
			xform_printf(OF, "\t\t\t\t<address>%s</address>\n", xml_escape(player->address));
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
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
	for ( ; player != NULL; player = player->next) {
		xform_printf(OF, "\t\t\t<player>\n");

		xform_printf(OF, "\t\t\t\t<name>%s</name>\n", xml_escape(xform_name(player->name, server)));
		xform_printf(OF, "\t\t\t\t<score>%d</score>\n", player->score);

		xform_printf(OF, "\t\t\t</player>\n");
	}

	xform_printf(OF, "\t\t</players>\n");
}


void
display_progress()
{
	static struct timeval rate_start =
	{
		0, 0
	};
	char rate[32];
	struct timeval now;

	gettimeofday(&now, NULL);

	if (!rate_start.tv_sec) {
		rate_start = now;
		rate[0] = '\0';
	} else {
		int delta = time_delta(&now, &rate_start);
		if (delta > 1500) {
			sprintf(rate, "  %d servers/sec  ", (num_servers_returned + num_servers_timed_out) * 1000 / delta);
		} else {
			rate[0] = '\0';
		}
	}

	// only print out every 'progress' number of servers.
	if ((0 != num_servers_returned + num_servers_timed_out) && ((progress == 1) || ((num_servers_returned + num_servers_timed_out) % progress == 0))) {
		fprintf(stderr, "\r%d/%d (%d timed out, %d down)%s", num_servers_returned + num_servers_timed_out, num_servers_total, num_servers_timed_out, num_servers_down, rate);
	}
}


/* ----- END MODIFICATION ----- Don't need to change anything below here. */

void set_non_blocking(int fd);
int set_fds(fd_set *fds);
void get_next_timeout(struct timeval *timeout);

void set_file_descriptors();
int wait_for_file_descriptors(struct timeval *timeout);
struct qserver *get_next_ready_server();

/* Misc flags
 */

struct timeval packet_recv_time;
int one_server_type_id = ~MASTER_SERVER;
static int one = 1;
static int little_endian;
static int big_endian;
unsigned int swap_long(void *);
unsigned short swap_short(void *);
float swap_float_from_little(void *f);

#define FORCE    1

/* Print an argument definition
 */
void
printf_opt(const char *opt, const char *desc_format, ...)
{
	va_list args;
	int i;

	printf("    %s", opt);

	for (i = strlen(opt); i < 20; i++) {
		printf(" ");
	}
	printf(" ");

	va_start(args, desc_format);
	vprintf(desc_format, args);
	va_end(args);
	printf("\n");
}


/* Print an error message and the program usage notes
 */
void
usage(char *msg, char **argv, char *a1)
{
	int i;
	server_type *type;
	server_type **sorted_types;

	if (msg) {
		fprintf(stderr, msg, a1);
	}

	printf("Usage: %s ", argv[0]);
	printf("[options] [-default server-type] [-cfg file] [-f file] [host[:port]] ...\n");
	printf("Where host is an IP address or host name\n\n");

	printf("Configuration options:\n");
	printf_opt("-cfg", "Read the extended types from given file not the default one");
	printf_opt("-nocfg", "Ignore qstat configuration loaded from any default location. Must be the first option on the command-line.");
	printf("\n");

	printf("Game query options:\n");

	sorted_types = (server_type **)malloc(sizeof(server_type *) * n_server_types);
	type = &types[0];
	for (i = 0; type->id != Q_UNKNOWN_TYPE && i < n_server_types; type++, i++) {
		sorted_types[i] = type;
	}

	quicksort((void **)sorted_types, 0, n_server_types - 1, (int (*)(void *, void *))type_option_compare);
	for (i = 0; i < n_server_types; i++) {
		type = sorted_types[i];
		printf_opt(type->type_option, "Query %s server", type->game_name);
	}
	printf_opt("-default", "Set default server type");
	printf("\n");

	printf("-default\tset default server type:");
	for (i = 0; i < n_server_types; i++) {
		type = sorted_types[i];
		printf(" %s", type->type_string);
	}
	printf("\n\n");

	printf("Content options:\n");
	printf_opt("-sort", "Sort servers and/or players");
	printf_opt("-u", "Only display servers that are up");
	printf_opt("-nf", "Do not display full servers");
	printf_opt("-ne", "Do not display empty servers");
	printf_opt("-R", "Fetch and display server rules");
	printf_opt("-P", "Fetch and display player info");
	printf("\n");

	printf("Format options:\n");
	printf_opt("-cn", "Display color names instead of numbers");
	printf_opt("-ncn", "Display color numbers instead of names");
	printf_opt("-hc", "Display colors in #rrggbb format");
	printf_opt("-htmlmode", "Convert <, >, and & to the equivalent HTML entities");
	printf_opt("-htmlnames", "Colorize Quake 3 and Tribes 2 player names using html font tags");
	printf_opt("-nohtmlnames", "Do not colorize Quake 3 and Tribes 2 player names even if $HTML is used in an output template.");
	printf_opt("-carets", "Display carets in Quake 3 player names");
	printf_opt("-hpn", "Display player names in hex");
	printf_opt("-hsn", "Display server names in hex");
	printf_opt("-tc", "Display time in clock format (DhDDmDDs)");
	printf_opt("-tsw", "Display time in stop-watch format (DD:DD:DD)");
	printf_opt("-ts", "Display time in seconds");
	printf_opt("-pa", "Display player address");
	printf("\n");

	printf("Display options:\n");
	printf_opt("-raw <delim>", "Output in raw format using <delim> as delimiter");
	printf_opt("-raw-arg", "When used with -raw, always display the server address as it appeared in a file or on the command-line.");
	printf_opt("-old", "Old style display");
	printf_opt("-nh", "Do not display header");
	printf_opt("-nx", "Enable name transformations, -nx is set by default");
	printf_opt("-nnx", "Disable name transformations, -utf8 option implies -nnx");
	printf_opt("-xml", "Output status data as an XML document");
	printf_opt("-bom", "Output Byte-Order-Mark for XML output.");
	printf_opt("-utf8", "Use the UTF-8 character encoding for XML output");
	printf_opt("-json", "Output status data as an UTF-8 JSON document");
	printf_opt("-Th,-Ts,-Tpt", "Output templates: header, server and player");
	printf_opt("-Tr,-Tt", "Output templates: rule, and trailer");
	printf_opt("-showgameport", "Always display the game port in QStat output.");
	printf_opt("-stripunprintable", "Disable stripping of unprintable characters.");
	printf_opt("-errors", "Display errors");
	printf_opt("-progress", "Display progress meter on stderr (text only)");
	printf("\n");

	printf("Output options:\n");
	printf_opt("-of", "Output file");
	printf_opt("-af", "Like -of, but append to the file");
	printf("\n");

	printf("Query options:\n");
	printf_opt("-f", "Read hosts from file");
	printf_opt("-mdelim <delim>", "For rules with multi values use <delim> as delimiter");
	printf_opt("-retry", "Number of retries, default is %d", DEFAULT_RETRIES);
	printf_opt("-interval", "Interval between retries, default is %.2f seconds", DEFAULT_RETRY_INTERVAL / 1000.0);
	printf_opt("-mi", "Interval between master server retries, default is %.2f seconds", (DEFAULT_RETRY_INTERVAL * 4) / 1000.0);
	printf_opt("-timeout", "Total time in seconds before giving up");
	printf_opt("-maxsim", "Set maximum simultaneous queries");
	printf_opt("-sendinterval", "Set time in ms between sending packets, default %u", sendinterval);
	printf_opt("-allowserverdups", "Allow adding multiple servers with same ip:port (needed for ts2)");
	printf_opt("-srcport <range>", "Send packets from these network ports");
	printf_opt("-srcip <IP>", "Send packets using this IP address");
	printf_opt("-H", "Resolve host names");
	printf_opt("-Hcache", "Host name cache file");
	printf("\n");

	printf("Advanced options:\n");
	printf_opt("-d", "Enable debug options. Specify multiple times to increase debug level.");
#ifdef ENABLE_DUMP
		printf_opt("-dump", "Write received raw packets to dumpNNN files which must not exist before");
		printf_opt("-pkt <file>", "Use file as server reply instead of quering the server. Works only with TF_SINGLE_QUERY servers");
#endif
	printf_opt("-syncconnect", "Process connect initialisation synchronously.");
	printf_opt("-noportoffset", "Dont use builtin status port offsets (assume query port was specified).");
#ifdef _WIN32
		printf_opt("-noconsole", "Free the console");
#endif
	printf("\n");

	printf("Help options:\n");
	printf_opt("-h, --help", "Print this help");
	printf_opt("-protocols", "Print the protocol list (json display format only)");
	printf_opt("-v", "Print qstat version (can be displayed with -json format)");
	printf("\n");

	printf("Sort keys:\n");
	printf("  servers: p=by-ping, g=by-game, i=by-IP-address, h=by-hostname, n=by-#-players, l=by-list-order\n");
	printf("  players: P=by-ping, F=by-frags, T=by-team, N=by-name\n");
	printf("\nqstat version %s\n", VERSION);
	exit(0);
}


struct server_arg {
	int type_id;
	server_type *type;
	char *arg;
	char *outfilename;
	char *query_arg;
};

server_type *
find_server_type_id(int type_id)
{
	server_type *type = &types[0];

	for ( ; type->id != Q_UNKNOWN_TYPE; type++) {
		if (type->id == type_id) {
			return (type);
		}
	}
	return (NULL);
}


server_type *
find_server_type_string(char *type_string)
{
	server_type *type = &types[0];
	char *t = type_string;

	for ( ; *t; t++) {
		*t = tolower(*t);
	}

	for ( ; type->id != Q_UNKNOWN_TYPE; type++) {
		if (strcmp(type->type_string, type_string) == 0) {
			return (type);
		}
	}
	return (NULL);
}


server_type *
find_server_type_option(char *option)
{
	server_type *type = &types[0];

	for ( ; type->id != Q_UNKNOWN_TYPE; type++) {
		if (strcmp(type->type_option, option) == 0) {
			return (type);
		}
	}
	return (NULL);
}


server_type *
parse_server_type_option(char *option, int *outfile, char **query_arg)
{
	server_type *type = &types[0];
	char *comma, *arg;
	int len;

	*outfile = 0;
	*query_arg = 0;

	comma = strchr(option, ',');
	if (comma) {
		*comma++ = '\0';
	}

	for ( ; type->id != Q_UNKNOWN_TYPE; type++) {
		if (strcmp(type->type_option, option) == 0) {
			break;
		}
	}

	if (type->id == Q_UNKNOWN_TYPE) {
		return (NULL);
	}

	if (!comma) {
		return (type);
	}

	if (strcmp(comma, "outfile") == 0) {
		*outfile = 1;
		comma = strchr(comma, ',');
		if (!comma) {
			return (type);
		}
		*comma++ = '\0';
	}

	*query_arg = strdup(comma);
	arg = comma;
	do {
		comma = strchr(arg, ',');
		if (comma) {
			len = comma - arg;
		} else {
			len = strlen(arg);
		}
		if (strncmp(arg, "outfile", len) == 0) {
			*outfile = 1;
		}
		arg = comma + 1;
	} while (comma);
	return (type);
}


void
add_server_arg(char *arg, int type, char *outfilename, char *query_arg, struct server_arg **args, int *n, int *max)
{
	if (*n == *max) {
		if (*max == 0) {
			*max = 4;
			*args = (struct server_arg *)malloc(sizeof(struct server_arg) * (*max));
		} else {
			(*max) *= 2;
			*args = (struct server_arg *)realloc(*args, sizeof(struct server_arg) * (*max));
		}
	}
	(*args)[*n].type_id = type;
	/*    (*args)[*n].type= find_server_type_id( type); */
	(*args)[*n].type = NULL;
	(*args)[*n].arg = arg;
	(*args)[*n].outfilename = outfilename;
	(*args)[*n].query_arg = query_arg;
	(*n)++;
}


void
add_query_param(struct qserver *server, char *arg)
{
	char *equal;
	struct query_param *param;

	equal = strchr(arg, '=');
	*equal++ = '\0';

	param = (struct query_param *)malloc(sizeof(struct query_param));
	param->key = arg;
	param->value = equal;
	sscanf(equal, "%i", &param->i_value);
	sscanf(equal, "%i", &param->ui_value);
	param->next = server->params;
	server->params = param;
}


char *
get_param_value(struct qserver *server, const char *key, char *default_value)
{
	struct query_param *p = server->params;

	for ( ; p; p = p->next) {
		if (strcasecmp(key, p->key) == 0) {
			return (p->value);
		}
	}
	return (default_value);
}


int
get_param_i_value(struct qserver *server, char *key, int default_value)
{
	struct query_param *p = server->params;

	for ( ; p; p = p->next) {
		if (strcasecmp(key, p->key) == 0) {
			return (p->i_value);
		}
	}
	return (default_value);
}


unsigned int
get_param_ui_value(struct qserver *server, char *key, unsigned int default_value)
{
	struct query_param *p = server->params;

	for ( ; p; p = p->next) {
		if (strcasecmp(key, p->key) == 0) {
			return (p->ui_value);
		}
	}
	return (default_value);
}


int
parse_source_address(char *addr, unsigned int *ip, unsigned short *port)
{
	char *colon;

	colon = strchr(addr, ':');
	if (colon) {
		*colon = '\0';
		*port = atoi(colon + 1);
		if (colon == addr) {
			return (0);
		}
	} else {
		*port = 0;
	}

	*ip = inet_addr(addr);
	if ((*ip == INADDR_NONE) && !isdigit((unsigned char)*ip)) {
		*ip = hcache_lookup_hostname(addr);
	}
	if (*ip == INADDR_NONE) {
		fprintf(stderr, "%s: Not an IP address or unknown host name\n", addr);
		return (-1);
	}
	*ip = ntohl(*ip);
	return (0);
}


int
parse_source_port(char *port, unsigned short *low, unsigned short *high)
{
	char *dash;

	*low = atoi(port);
	dash = strchr(port, '-');
	*high = 0;
	if (dash) {
		*high = atoi(dash + 1);
	}
	if (*high == 0) {
		*high = *low;
	}

	if (*high < *low) {
		fprintf(stderr, "%s: Invalid port range\n", port);
		return (-1);
	}
	return (0);
}


void
add_config_server_types()
{
	int n_config_types, n_builtin_types, i;
	server_type **config_types;
	server_type *new_types, *type;

	config_types = qsc_get_config_server_types(&n_config_types);

	if (n_config_types == 0) {
		return;
	}

	n_builtin_types = (sizeof(builtin_types) / sizeof(server_type)) - 1;
	new_types = (server_type *)malloc(sizeof(server_type) * (n_builtin_types + n_config_types + 1));

	memcpy(new_types, &builtin_types[0], n_builtin_types * sizeof(server_type));
	type = &new_types[n_builtin_types];
	for (i = n_config_types; i; i--, config_types++, type++) {
		*type = **config_types;
	}
	n_server_types = n_builtin_types + n_config_types;
	new_types[n_server_types].id = Q_UNKNOWN_TYPE;
	if (types != &builtin_types[0]) {
		free(types);
	}
	types = new_types;
}


void
revert_server_types()
{
	if (types != &builtin_types[0]) {
		free(types);
	}
	n_server_types = (sizeof(builtin_types) / sizeof(server_type)) - 1;
	types = &builtin_types[0];
}


#ifdef ENABLE_DUMP
	unsigned pkt_dump_pos = 0;
	const char *pkt_dumps[64] = { 0 };
	static void
	add_pkt_from_file(const char *file)
	{
		if (pkt_dump_pos >= sizeof(pkt_dumps) / sizeof(pkt_dumps[0])) {
			return;
		}
		pkt_dumps[pkt_dump_pos++] = file;
	}


	static void
	replay_pkt_dumps()
	{
		struct qserver *server = servers;
		char *pkt = NULL;
		int fd;
		int bytes_read = 0; // should be ssize_t but for ease with win32
		int i;
		struct stat statbuf;

		gettimeofday(&packet_recv_time, NULL);

		for (i = 0; i < pkt_dump_pos; i++) {
			if ((fd = open(pkt_dumps[i], O_RDONLY)) == -1) {
				goto err;
			}

			if (fstat(fd, &statbuf) == -1) {
				goto err;
			}
			pkt = malloc(statbuf.st_size);
			if (NULL == pkt) {
				goto err;
			}
			bytes_read = read(fd, pkt, statbuf.st_size);
			if (bytes_read != statbuf.st_size) {
				fprintf(stderr, "Failed to read entire packet from disk got %d of %ld bytes\n", bytes_read, (long)statbuf.st_size);
				goto err;
			}
			close(fd);
			fd = 0;

			debug(2, "replay, pre-packet_func");
			process_func_ret(server, server->type->packet_func(server, pkt, statbuf.st_size));
			debug(2, "replay, post-packet_func");
		}
		goto out;

err:            perror(__FUNCTION__);
		close(fd);
out:            fd = 0; // NOP
	}


#endif  // ENABLE_DUMP

struct rcv_pkt {
	struct qserver *server;
	struct sockaddr_in addr;
	struct timeval recv_time;
	char data[PACKET_LEN];
	int len;
	int _errno;
};

void
do_work(void)
{
	int pktlen, rc;
	char *pkt = NULL;
	int bind_retry = 0;
	struct timeval timeout;
	struct rcv_pkt *buffer;
	unsigned buffill = 0, i = 0;
	unsigned bufsize = max_simultaneous * 2;

	struct timeval t, ts;

	gettimeofday(&t, NULL);
	ts = t;

	buffer = malloc(sizeof(struct rcv_pkt) * bufsize);

	if (!buffer) {
		return;
	}

#ifdef ENABLE_DUMP
		if (pkt_dump_pos) {
			replay_pkt_dumps();
		} else
#endif
	{
		bind_retry = bind_sockets();
	}

	send_packets();

	debug(2, "connected: %d", connected);

	while (connected || (!connected && bind_retry == -2)) {
		if (!connected && (bind_retry == -2)) {
			rc = wait_for_timeout(60);
			bind_retry = bind_sockets();
			continue;
		}
		bind_retry = 0;

		set_file_descriptors();

		if (progress) {
			display_progress();
		}

		get_next_timeout(&timeout);

		rc = wait_for_file_descriptors(&timeout);

		debug(2, "rc %d", rc);

		if (rc == SOCKET_ERROR) {
#ifndef _WIN32
				if (errno == EINTR) {
					continue;
				}
#endif
			perror("select");
			break;
		}

		for ( ; rc && buffill < bufsize; rc--) {
			int addrlen = sizeof(buffer[buffill].addr);
			struct qserver *server = get_next_ready_server();
			if (server == NULL) {
				break;
			}

			gettimeofday(&buffer[buffill].recv_time, NULL);

			pktlen = recvfrom(server->fd, buffer[buffill].data, sizeof(buffer[buffill].data), 0, (struct sockaddr *)&buffer[buffill].addr, (void *)&addrlen);

			debug(2, "recvfrom: %d", pktlen);

			// pktlen == 0 is no error condition! happens on remote tcp socket close
			if (pktlen == SOCKET_ERROR) {
				if (connection_would_block()) {
					malformed_packet(server, "EAGAIN on UDP socket, probably incorrect checksum");
				} else if (connection_refused() || connection_reset()) {
					server->server_name = DOWN;
					num_servers_down++;
					cleanup_qserver(server, FORCE);
				}
				continue;
			}

			debug(1, "recv %3d %3d %d.%d.%d.%d:%hu\n",
			    time_delta(&buffer[buffill].recv_time, &ts),
			    time_delta(&buffer[buffill].recv_time, &t),
			    server->ipaddr & 0xff,
			    (server->ipaddr >> 8) & 0xff,
			    (server->ipaddr >> 16) & 0xff,
			    (server->ipaddr >> 24) & 0xff,
			    server->port
			    );

			t = buffer[buffill].recv_time;

			buffer[buffill].server = server;
			buffer[buffill].len = pktlen;
			++buffill;
		}

		debug(2, "fill: %d < %d", buffill, bufsize);

		for (i = 0; i < buffill; ++i) {
			struct qserver *server = buffer[i].server;
			pkt = buffer[i].data;
			pktlen = buffer[i].len;
			memcpy(&packet_recv_time, &buffer[i].recv_time, sizeof(packet_recv_time));

			if (get_debug_level() > 2) {
				print_packet(server, pkt, pktlen);
			}

#ifdef ENABLE_DUMP
				if (do_dump) {
					dump_packet(pkt, pktlen);
				}
#endif
			if (server->flags & FLAG_BROADCAST) {
				struct qserver *broadcast = server;
				unsigned short port = ntohs(buffer[i].addr.sin_port);
				/* create new server and init */
				if (!(no_port_offset || server->flags & TF_NO_PORT_OFFSET)) {
					port -= server->type->port_offset;
				}
				server = add_qserver_byaddr(ntohl(buffer[i].addr.sin_addr.s_addr), port, server->type, NULL);
				if (server == NULL) {
					server = find_server_by_address(buffer[i].addr.sin_addr.s_addr, ntohs(buffer[i].addr.sin_port));
					if (server == NULL) {
						continue;
					}

					/*
					 * if ( show_errors)
					 * {
					 * fprintf(stderr,
					 * "duplicate or invalid packet received from 0x%08x:%hu\n",
					 * ntohl(buffer[i].addr.sin_addr.s_addr), ntohs(buffer[i].addr.sin_port));
					 * print_packet( NULL, pkt, pktlen);
					 * }
					 * continue;
					 */
				} else {
					server->packet_time1 = broadcast->packet_time1;
					server->packet_time2 = broadcast->packet_time2;
					server->ping_total = broadcast->ping_total;
					server->n_requests = broadcast->n_requests;
					server->n_packets = broadcast->n_packets;
					broadcast->n_servers++;
				}
			}

			debug(2, "connected, pre-packet_func: %d", connected);
			process_func_ret(server, server->type->packet_func(server, pkt, pktlen));
			debug(2, "connected, post-packet_func: %d", connected);
		}
		buffill = 0;

		if (run_timeout && (time(0) - start_time >= run_timeout)) {
			debug(2, "run timeout reached");
			break;
		}

		send_packets();
		if (connected < max_simultaneous) {
			bind_retry = bind_sockets();
		}

		debug(2, "connected: %d", connected);
	}

	free(buffer);
}


int
main(int argc, char *argv[])
{
	int arg, n_files, i;
	char **files, *outfilename, *query_arg;
	struct server_arg *server_args = NULL;
	int n_server_args = 0, max_server_args = 0;
	int default_server_type_id;

#ifdef _WIN32
		WORD version = MAKEWORD(1, 1);
		WSADATA wsa_data;
		if (WSAStartup(version, &wsa_data) != 0) {
			fprintf(stderr, "Could not open winsock\n");
			exit(1);
		}
#else
		signal(SIGPIPE, SIG_IGN);
#endif

	types = &builtin_types[0];
	n_server_types = (sizeof(builtin_types) / sizeof(server_type)) - 1;

	i = qsc_load_default_config_files();
	if (i == -1) {
		return (1);
	} else if (i == 0) {
		add_config_server_types();
	}

	if (argc == 1) {
		usage(NULL, argv, NULL);
	}

	OF = stdout;

	files = (char **)malloc(sizeof(char *) * (argc / 2));
	n_files = 0;

	default_server_type_id = Q_SERVER;
	little_endian = ((char *)&one)[0];
	big_endian = !little_endian;

	for (arg = 1; arg < argc; arg++) {
		if (argv[arg][0] != '-') {
			break;
		}
		outfilename = NULL;
		if ((strcmp(argv[arg], "-nocfg") == 0) && (arg == 1)) {
			revert_server_types();
		} else if (strcmp(argv[arg], "--help") == 0) {
			usage(NULL, argv, NULL);
		} else if (strcmp(argv[arg], "-h") == 0) {
			usage(NULL, argv, NULL);
		} else if (strcmp(argv[arg], "-protocols") == 0) {
			if (json_display == 1) {
				json_protocols();
			} else {
				fprintf(stderr, "must be used with -json\n");
				exit(1);
			}
		} else if (strcmp(argv[arg], "-v") == 0) {
			if (json_display == 1) {
				json_version();
			} else {
				printf("%s\n", VERSION);
			}
		} else if (strcmp(argv[arg], "-f") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -f\n", argv, NULL);
			}
			files[n_files++] = argv[arg];
		} else if (strcmp(argv[arg], "-retry") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -retry\n", argv, NULL);
			}
			n_retries = atoi(argv[arg]);
			if (n_retries <= 0) {
				fprintf(stderr, "retries must be greater than zero\n");
				exit(1);
			}
		} else if (strcmp(argv[arg], "-interval") == 0) {
			double value = 0.0;
			arg++;
			if (arg >= argc) {
				usage("missing argument for -interval\n", argv, NULL);
			}
			sscanf(argv[arg], "%lf", &value);
			if (value < 0.1) {
				fprintf(stderr, "retry interval must be greater than 0.1\n");
				exit(1);
			}
			retry_interval = (int)(value * 1000);
		} else if (strcmp(argv[arg], "-mi") == 0) {
			double value = 0.0;
			arg++;
			if (arg >= argc) {
				usage("missing argument for -mi\n", argv, NULL);
			}
			sscanf(argv[arg], "%lf", &value);
			if (value < 0.1) {
				fprintf(stderr, "interval must be greater than 0.1\n");
				exit(1);
			}
			master_retry_interval = (int)(value * 1000);
		} else if (strcmp(argv[arg], "-H") == 0) {
			hostname_lookup = 1;
		} else if (strcmp(argv[arg], "-u") == 0) {
			up_servers_only = 1;
		} else if (strcmp(argv[arg], "-nf") == 0) {
			no_full_servers = 1;
		} else if (strcmp(argv[arg], "-ne") == 0) {
			no_empty_servers = 1;
		} else if (strcmp(argv[arg], "-nh") == 0) {
			no_header_display = 1;
		} else if (strcmp(argv[arg], "-old") == 0) {
			new_style = 0;
		} else if (strcmp(argv[arg], "-P") == 0) {
			get_player_info = 1;
		} else if (strcmp(argv[arg], "-R") == 0) {
			get_server_rules = 1;
		} else if (strncmp(argv[arg], "-raw", 4) == 0) {
			if (json_display == 1) {
				usage("cannot specify both -json and -raw\n", argv, NULL);
			}
			if (xml_display == 1) {
				usage("cannot specify both -xml and -raw\n", argv, NULL);
			}
			if (argv[arg][4] == ',') {
				if (strcmp(&argv[arg][5], "game") == 0) {
					show_game_in_raw = 1;
				} else {
					usage("Unknown -raw option\n", argv, NULL);
				}
			}
			arg++;
			if (arg >= argc) {
				usage("missing argument for -raw\n", argv, NULL);
			}
			raw_delimiter = argv[arg];

			// Check the multi rule delimiter isnt the same
			// If it is fix to maintain backwards compatibility
			if ((0 == strcmp(raw_delimiter, multi_delimiter)) && (0 == strcmp(raw_delimiter, "|"))) {
				multi_delimiter = ":";
			}
			raw_display = 1;
		} else if (strcmp(argv[arg], "-mdelim") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -mdelim\n", argv, NULL);
			}
			multi_delimiter = argv[arg];
		} else if (strcmp(argv[arg], "-xml") == 0) {
			xml_display = 1;
			if (raw_display == 1) {
				usage("cannot specify both -raw and -xml\n", argv, NULL);
			}
			if (json_display == 1) {
				usage("cannot specify both -json and -xml\n", argv, NULL);
			}
		} else if (strcmp(argv[arg], "-json") == 0) {
			json_display = 1;
			if (raw_display == 1) {
				usage("cannot specify both -raw and -json\n", argv, NULL);
			}
			if (xml_display == 1) {
				usage("cannot specify both -xml and -json\n", argv, NULL);
			}
		} else if (strcmp(argv[arg], "-utf8") == 0) {
			xml_encoding = ENCODING_UTF_8;
			xform_names = 0;
		} else if (strcmp(argv[arg], "-ncn") == 0) {
			color_names = 0;
		} else if (strcmp(argv[arg], "-cn") == 0) {
			color_names = 1;
		} else if (strcmp(argv[arg], "-hc") == 0) {
			color_names = 2;
		} else if (strcmp(argv[arg], "-nx") == 0) {
			xform_names = 1;
		} else if (strcmp(argv[arg], "-nnx") == 0) {
			xform_names = 0;
		} else if (strcmp(argv[arg], "-tc") == 0) {
			time_format = CLOCK_TIME;
		} else if (strcmp(argv[arg], "-tsw") == 0) {
			time_format = STOPWATCH_TIME;
		} else if (strcmp(argv[arg], "-ts") == 0) {
			time_format = SECONDS;
		} else if (strcmp(argv[arg], "-pa") == 0) {
			player_address = 1;
		} else if (strcmp(argv[arg], "-hpn") == 0) {
			xform_hex_player_names = 1;
		} else if (strcmp(argv[arg], "-hsn") == 0) {
			xform_hex_server_names = 1;
		} else if (strncmp(argv[arg], "-maxsimultaneous", 7) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -maxsimultaneous\n", argv, NULL);
			}
			max_simultaneous = atoi(argv[arg]);
			if (max_simultaneous <= 0) {
				usage("value for -maxsimultaneous must be > 0\n", argv, NULL);
			}
			if (max_simultaneous > FD_SETSIZE) {
				max_simultaneous = FD_SETSIZE;
			}
		} else if (strcmp(argv[arg], "-sendinterval") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -sendinterval\n", argv, NULL);
			}
			sendinterval = atoi(argv[arg]);
			if (sendinterval < 0) {
				usage("value for -sendinterval must be >= 0\n", argv, NULL);
			}
		} else if (strcmp(argv[arg], "-raw-arg") == 0) {
			raw_arg = 1000;
		} else if (strcmp(argv[arg], "-timeout") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -timeout\n", argv, NULL);
			}
			run_timeout = atoi(argv[arg]);
			if (run_timeout <= 0) {
				usage("value for -timeout must be > 0\n", argv, NULL);
			}
		} else if (strncmp(argv[arg], "-progress", sizeof("-progress") - 1) == 0) {
			char *p = argv[arg] + sizeof("-progress") - 1;
			progress = 1;
			if (*p == ',') {
				progress = atoi(p + 1);
			}
		} else if (strcmp(argv[arg], "-Hcache") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -Hcache\n", argv, NULL);
			}
			if (hcache_open(argv[arg], 0) == -1) {
				return (1);
			}
		} else if (strcmp(argv[arg], "-default") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for -default\n", argv, NULL);
			}
			default_server_type = find_server_type_string(argv[arg]);
			if (default_server_type == NULL) {
				char opt[256], *o = &opt[0];
				sprintf(opt, "-%s", argv[arg]);
				for ( ; *o; o++) {
					*o = tolower(*o);
				}
				default_server_type = find_server_type_option(opt);
			}
			if (default_server_type == NULL) {
				fprintf(stderr, "unknown server type \"%s\"\n", argv[arg]);
				usage(NULL, argv, NULL);
			}
			default_server_type_id = default_server_type->id;
			default_server_type = NULL;
		} else if (strncmp(argv[arg], "-Tserver", 3) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (read_qserver_template(argv[arg]) == -1) {
				return (1);
			}
		} else if (strncmp(argv[arg], "-Trule", 3) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (read_rule_template(argv[arg]) == -1) {
				return (1);
			}
		} else if (strncmp(argv[arg], "-Theader", 3) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (read_header_template(argv[arg]) == -1) {
				return (1);
			}
		} else if (strncmp(argv[arg], "-Ttrailer", 3) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (read_trailer_template(argv[arg]) == -1) {
				return (1);
			}
		} else if (strncmp(argv[arg], "-Tplayer", 3) == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (read_player_template(argv[arg]) == -1) {
				return (1);
			}
		} else if (strcmp(argv[arg], "-sort") == 0) {
			size_t pos;
			arg++;
			if (arg >= argc) {
				usage("missing argument for -sort\n", argv, NULL);
			}
			strncpy(sort_keys, argv[arg], sizeof(sort_keys) - 1);
			pos = strspn(sort_keys, SUPPORTED_SORT_KEYS);
			if (pos != strlen(sort_keys)) {
				fprintf(stderr, "Unknown sort key \"%c\", valid keys are \"%s\"\n", sort_keys[pos], SUPPORTED_SORT_KEYS);
				return (1);
			}
			server_sort = strpbrk(sort_keys, SUPPORTED_SERVER_SORT) != NULL;
			if (strchr(sort_keys, 'l')) {
				server_sort = 1;
			}
			player_sort = strpbrk(sort_keys, SUPPORTED_PLAYER_SORT) != NULL;
		} else if (strcmp(argv[arg], "-errors") == 0) {
			show_errors++;
		} else if (strcmp(argv[arg], "-of") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if ((argv[arg][0] == '-') && (argv[arg][1] == '\0')) {
				OF = stdout;
			} else {
				OF = fopen(argv[arg], "w");
			}
			if (OF == NULL) {
				perror(argv[arg]);
				return (1);
			}
		} else if (strcmp(argv[arg], "-af") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if ((argv[arg][0] == '-') && (argv[arg][1] == '\0')) {
				OF = stdout;
			} else {
				OF = fopen(argv[arg], "a");
			}
			if (OF == NULL) {
				perror(argv[arg]);
				return (1);
			}
		} else if (strcmp(argv[arg], "-htmlnames") == 0) {
			xform_html_names = 1;
		} else if (strcmp(argv[arg], "-nohtmlnames") == 0) {
			xform_html_names = 0;
		} else if (strcmp(argv[arg], "-htmlmode") == 0) {
			html_mode = 1;
		} else if (strcmp(argv[arg], "-carets") == 0) {
			xform_strip_carets = 0;
		} else if (strcmp(argv[arg], "-d") == 0) {
			set_debug_level(get_debug_level() + 1);
		} else if (strcmp(argv[arg], "-showgameport") == 0) {
			show_game_port = 1;
		} else if (strcmp(argv[arg], "-noportoffset") == 0) {
			no_port_offset = 1;
		} else if (strcmp(argv[arg], "-srcip") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (parse_source_address(argv[arg], &source_ip, &source_port) == -1) {
				return (1);
			}
			if (source_port) {
				source_port_low = source_port;
				source_port_high = source_port;
			}
		} else if (strcmp(argv[arg], "-syncconnect") == 0) {
			syncconnect = 1;
		} else if (strcmp(argv[arg], "-stripunprintable") == 0) {
			xform_strip_unprintable = 1;
		} else if (strcmp(argv[arg], "-srcport") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (parse_source_port(argv[arg], &source_port_low, &source_port_high) == -1) {
				return (1);
			}
			source_port = source_port_low;
		} else if (strcmp(argv[arg], "-cfg") == 0) {
			arg++;
			if (arg >= argc) {
				usage("missing argument for %s\n", argv, argv[arg - 1]);
			}
			if (qsc_load_config_file(argv[arg]) == -1) {
				return (1);
			}
			add_config_server_types();
		} else if (strcmp(argv[arg], "-allowserverdups") == 0) {
			noserverdups = 0;
		} else if (strcmp(argv[arg], "-bom") == 0) {
			output_bom = 1;
		}
#ifdef ENABLE_DUMP
			else if (strcmp(argv[arg], "-dump") == 0) {
				do_dump = 1;
			} else if (strcmp(argv[arg], "-pkt") == 0) {
				arg++;
				if (arg >= argc) {
					usage("missing argument for %s\n", argv, argv[arg - 1]);
				}
				add_pkt_from_file(argv[arg]);
			}
#endif
#ifdef _WIN32
			else if (strcmp(argv[arg], "-noconsole") == 0) {
				FreeConsole();
			}
#endif
		else {
			int outfile;
			server_type *type;
			arg++;
			if (arg >= argc) {
				fprintf(stderr, "missing argument for \"%s\"\n", argv[arg - 1]);
				return (1);
			}
			type = parse_server_type_option(argv[arg - 1], &outfile, &query_arg);
			if (type == NULL) {
				fprintf(stderr, "unknown option \"%s\"\n", argv[arg - 1]);
				return (1);
			}
			outfilename = NULL;
			if (outfile) {
				outfilename = strchr(argv[arg], ',');
				if (outfilename == NULL) {
					fprintf(stderr, "missing file name for \"%s,outfile\"\n", argv[arg - 1]);
					return (1);
				}
				*outfilename++ = '\0';
			}

			/*
			 * if ( query_arg && !(type->flags & TF_QUERY_ARG)) {
			 * fprintf( stderr, "option flag \"%s\" not allowed for this server type\n",
			 * query_arg);
			 * return 1;
			 * }
			 */
			if (type->flags & TF_QUERY_ARG_REQUIRED && !query_arg) {
				fprintf(stderr, "option flag missing for server type \"%s\"\n", argv[arg - 1]);
				return (1);
			}
			add_server_arg(argv[arg], type->id, outfilename, query_arg, &server_args, &n_server_args, &max_server_args);
		}
	}

	start_time = time(0);

	default_server_type = find_server_type_id(default_server_type_id);

	for (i = 0; i < n_files; i++) {
		add_file(files[i]);
	}

	for ( ; arg < argc; arg++) {
		add_qserver(argv[arg], default_server_type, NULL, NULL);
	}

	for (i = 0; i < n_server_args; i++) {
		server_type *server_type = find_server_type_id(server_args[i].type_id);
		add_qserver(server_args[i].arg, server_type, server_args[i].outfilename, server_args[i].query_arg);
	}

	free(server_args);

	if (servers == NULL) {
		exit(1);
	}

	max_connmap = max_simultaneous + 10;
	connmap = (struct qserver **)calloc(1, sizeof(struct qserver *) * max_connmap);

	if (color_names == -1) {
		color_names = (raw_display) ? DEFAULT_COLOR_NAMES_RAW : DEFAULT_COLOR_NAMES_DISPLAY;
	}

	if (time_format == -1) {
		time_format = (raw_display) ? DEFAULT_TIME_FMT_RAW : DEFAULT_TIME_FMT_DISPLAY;
	}

	if ((one_server_type_id & MASTER_SERVER) || (one_server_type_id == 0)) {
		display_prefix = 1;
	}

	if (xml_display) {
		xml_header();
	} else if (json_display) {
		json_header();
	} else if (new_style && !raw_display && !have_server_template()) {
		display_header();
	} else if (have_header_template()) {
		template_display_header();
	}

	q_serverinfo.length = htons(q_serverinfo.length);
	h2_serverinfo.length = htons(h2_serverinfo.length);
	q_player.length = htons(q_player.length);

	do_work();

	finish_output();
	free_server_hash();
	free(files);
	free(connmap);

	return (0);
}


void
finish_output()
{
	int i;

	hcache_update_file();

	if (progress) {
		display_progress();
		fputs("\n", stderr);
	}

	if (server_sort) {
		struct qserver **array, *server, *next_server;
		if (strchr(sort_keys, 'l') && (strpbrk(sort_keys, SUPPORTED_SERVER_SORT) == NULL)) {
			server = servers;
			for ( ; server; server = next_server) {
				next_server = server->next;
				display_server(server);
			}
		} else {
			array = (struct qserver **)malloc(sizeof(struct qserver *) * num_servers_total);
			server = servers;
			for (i = 0; server != NULL; i++) {
				array[i] = server;
				server = server->next;
			}
			sort_servers(array, num_servers_total);
			if (progress) {
				fprintf(stderr, "\n");
			}
			for (i = 0; i < num_servers_total; i++) {
				display_server(array[i]);
			}
			free(array);
		}
	} else {
		struct qserver *server, *next_server;
		server = servers;
		for ( ; server; server = next_server) {
			next_server = server->next;
			if (server->server_name == HOSTNOTFOUND) {
				display_server(server);
			}
		}
	}

	if (xml_display) {
		xml_footer();
	} else if (json_display) {
		json_footer();
	} else if (have_trailer_template()) {
		template_display_trailer();
	}

	if (OF != stdout) {
		fclose(OF);
	}
}


void
add_file(char *filename)
{
	FILE *file;
	char name[200], *comma, *query_arg = NULL;
	server_type *type;

	debug(4, "Loading servers from '%s'...\n", filename);

	if (strcmp(filename, "-") == 0) {
		file = stdin;
		current_filename = NULL;
	} else {
		file = fopen(filename, "r");
		current_filename = filename;
	}
	current_fileline = 1;

	if (file == NULL) {
		perror(filename);
		return;
	}
	for ( ; fscanf(file, "%s", name) == 1; current_fileline++) {
		comma = strchr(name, ',');
		if (comma) {
			*comma++ = '\0';
			query_arg = strdup(comma);
		}
		type = find_server_type_string(name);
		if (type == NULL) {
			add_qserver(name, default_server_type, NULL, NULL);
		} else if (fscanf(file, "%s", name) == 1) {
			if (type->flags & TF_QUERY_ARG && comma && *query_arg) {
				add_qserver(name, type, NULL, query_arg);
			} else {
				add_qserver(name, type, NULL, NULL);
			}
		}
	}

	if (file != stdin) {
		fclose(file);
	}
	debug(4, "Loaded servers from '%s'\n", filename);

	current_fileline = 0;
}


void
print_file_location()
{
	if (current_fileline != 0) {
		fprintf(stderr, "%s:%d: ", current_filename ? current_filename : "<stdin>", current_fileline);
	}
}


void
parse_query_params(struct qserver *server, char *params)
{
	char *comma, *arg = params;

	do {
		comma = strchr(arg, ',');
		if (comma) {
			*comma = '\0';
		}
		if (strchr(arg, '=')) {
			add_query_param(server, arg);
		} else if ((strcmp(arg, "noportoffset") == 0) || (strcmp(arg, "qp") == 0)) {
			server->flags |= TF_NO_PORT_OFFSET;
		} else if ((strcmp(arg, "showgameport") == 0) || (strcmp(arg, "gp") == 0)) {
			server->flags |= TF_SHOW_GAME_PORT;
		}
		arg = comma + 1;
	} while (comma);
}


int
add_qserver(char *arg, server_type *type, char *outfilename, char *query_arg)
{
	struct qserver *server, *prev_server;
	int flags = 0;
	char *colon = NULL, *arg_copy, *hostname = NULL;
	unsigned int ipaddr;
	unsigned short port, port_max;
	int portrange = 0;
	unsigned colonpos = 0;

	debug(4, "%s, %s, %s, %s\n", arg, (NULL != type) ? type->type_string : "unknown", outfilename, query_arg);

	if (run_timeout && (time(0) - start_time >= run_timeout)) {
		finish_output();
		exit(0);
	}

	port = port_max = type->default_port;

	if (outfilename && (strcmp(outfilename, "-") != 0)) {
		FILE *outfile = fopen(outfilename, "r+");
		if ((outfile == NULL) && ((errno == EACCES) || (errno == EISDIR) || (errno == ENOSPC) || (errno == ENOTDIR))) {
			perror(outfilename);
			return (-1);
		}
		if (outfile) {
			fclose(outfile);
		}
	}

	arg_copy = strdup(arg);

	colon = strchr(arg, ':');
	if (colon != NULL) {
		if (sscanf(colon + 1, "%hu-%hu", &port, &port_max) == 2) {
			portrange = 1;
		} else {
			port_max = port;
		}
		*colon = '\0';
		colonpos = colon - arg;
	}

	if (*arg == '+') {
		flags |= FLAG_BROADCAST;
		arg++;
	}

	ipaddr = inet_addr(arg);
	if (ipaddr == INADDR_NONE) {
		if (strcmp(arg, "255.255.255.255") != 0) {
			ipaddr = htonl(hcache_lookup_hostname(arg));
		}
	} else if (hostname_lookup && !(flags & FLAG_BROADCAST)) {
		hostname = hcache_lookup_ipaddr(ntohl(ipaddr));
	}

	if (((ipaddr == INADDR_NONE) || (ipaddr == 0)) && (strcmp(arg, "255.255.255.255") != 0)) {
		if (show_errors) {
			print_file_location();
			fprintf(stderr, "%s: %s\n", arg, strherror(h_errno));
		}
		server = (struct qserver *)calloc(1, sizeof(struct qserver));
		// NOTE: 0 != port to prevent infinite loop due to lack of range on unsigned short
		for ( ; port <= port_max && 0 != port; ++port) {
			init_qserver(server, type);
			if (portrange) {
				server->arg = (port == port_max) ? arg_copy : strdup(arg_copy);
				// NOTE: arg_copy and therefore server->arg will always have enough space as it was a port range
				sprintf(server->arg + colonpos + 1, "%hu", port);
			} else {
				server->arg = arg_copy;
			}
			server->server_name = HOSTNOTFOUND;
			server->error = strdup(strherror(h_errno));
			server->orig_port = server->query_port = server->port = port;
			if (last_server != &servers) {
				prev_server = (struct qserver *)((char *)last_server - ((char *)&server->next - (char *)server));
				server->prev = prev_server;
			}
			*last_server = server;
			last_server = &server->next;
			if (one_server_type_id == ~MASTER_SERVER) {
				one_server_type_id = type->id;
			} else if (one_server_type_id != type->id) {
				one_server_type_id = 0;
			}
		}
		return (-1);
	}

	// NOTE: 0 != port to prevent infinite loop due to lack of range on unsigned short
	for ( ; port <= port_max && 0 != port; ++port) {
		if (noserverdups && (find_server_by_address(ipaddr, port) != NULL)) {
			continue;
		}

		server = (struct qserver *)calloc(1, sizeof(struct qserver));
		server->arg = port == port_max ? arg_copy : strdup(arg_copy);
		if (portrange) {
			sprintf(server->arg + colonpos + 1, "%hu", port);
		}
		if (hostname && colon) {
			server->host_name = (char *)malloc(strlen(hostname) + 5 + 2);
			sprintf(server->host_name, "%s:%hu", hostname, port);
		} else {
			server->host_name = strdup((hostname) ? hostname : arg);
		}

		server->ipaddr = ipaddr;
		server->orig_port = server->query_port = server->port = port;
		server->type = type;
		server->outfilename = outfilename;
		server->flags = flags;
		server->state = STATE_INIT;
		if (query_arg) {
			server->query_arg = (port == port_max) ? query_arg : strdup(query_arg);
			parse_query_params(server, server->query_arg);
		} else {
			server->query_arg = NULL;
		}
		init_qserver(server, type);

		if (server->type->master) {
			waiting_for_masters++;
		}

		if (num_servers_total % 10 == 0) {
			hcache_update_file();
		}

		if (last_server != &servers) {
			prev_server = (struct qserver *)((char *)last_server - ((char *)&server->next - (char *)server));
			server->prev = prev_server;
		}
		*last_server = server;
		last_server = &server->next;

		add_server_to_hash(server);

		if (one_server_type_id == ~MASTER_SERVER) {
			one_server_type_id = type->id;
		} else if (one_server_type_id != type->id) {
			one_server_type_id = 0;
		}

		++num_servers;
	}
	return (0);
}


struct qserver *
add_qserver_byaddr(unsigned int ipaddr, unsigned short port, server_type *type, int *new_server)
{
	char arg[36];
	struct qserver *server, *prev_server;
	char *hostname = NULL;

	if (run_timeout && (time(0) - start_time >= run_timeout)) {
		finish_output();
		exit(0);
	}

	if (new_server) {
		*new_server = 0;
	}
	ipaddr = htonl(ipaddr);
	if (ipaddr == 0) {
		return (0);
	}

	// TODO: this prevents servers with the same ip:port being queried
	// and hence breaks virtual servers support e.g. Teamspeak 2
	if (find_server_by_address(ipaddr, port) != NULL) {
		return (0);
	}

	if (new_server) {
		*new_server = 1;
	}

	server = (struct qserver *)calloc(1, sizeof(struct qserver));
	server->ipaddr = ipaddr;
	ipaddr = ntohl(ipaddr);
	sprintf(arg, "%d.%d.%d.%d:%hu", ipaddr >> 24, (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff, port);
	server->arg = strdup(arg);

	if (hostname_lookup) {
		hostname = hcache_lookup_ipaddr(ipaddr);
	}

	if (hostname) {
		server->host_name = (char *)malloc(strlen(hostname) + 6 + 2);
		sprintf(server->host_name, "%s:%hu", hostname, port);
	} else {
		server->host_name = strdup(arg);
	}

	server->orig_port = server->query_port = server->port = port;
	init_qserver(server, type);

	if (num_servers_total % 10 == 0) {
		hcache_update_file();
	}

	if (last_server != &servers) {
		prev_server = (struct qserver *)((char *)last_server - ((char *)&server->next - (char *)server));
		server->prev = prev_server;
	}
	*last_server = server;
	last_server = &server->next;

	add_server_to_hash(server);

	++num_servers;

	return (server);
}


void
add_servers_from_masters()
{
	struct qserver *server;
	unsigned int ipaddr, i;
	unsigned short port;
	int n_servers, new_server, port_adjust = 0;
	char *pkt;
	server_type *server_type;
	FILE *outfile;

	for (server = servers; server != NULL; server = server->next) {
		if (!server->type->master || (server->master_pkt == NULL)) {
			continue;
		}
		pkt = server->master_pkt;

		if (server->query_arg && (server->type->id == GAMESPY_MASTER)) {
			server_type = find_server_type_string(server->query_arg);
			if (server_type == NULL) {
				server_type = find_server_type_id(server->type->master);
			}
		} else {
			server_type = find_server_type_id(server->type->master);
		}

		if ((server->type->id == GAMESPY_MASTER) && server_type) {
			if (server_type->id == UN_SERVER) {
				port_adjust = -1;
			} else if (server_type->id == KINGPIN_SERVER) {
				port_adjust = 10;
			}
		}

		outfile = NULL;
		if (server->outfilename) {
			if (strcmp(server->outfilename, "-") == 0) {
				outfile = stdout;
			} else {
				outfile = fopen(server->outfilename, "w");
			}
			if (outfile == NULL) {
				perror(server->outfilename);
				continue;
			}
		}
		n_servers = 0;
		for (i = 0; i < server->master_pkt_len; i += 6) {
			memcpy(&ipaddr, &pkt[i], 4);
			memcpy(&port, &pkt[i + 4], 2);
			ipaddr = ntohl(ipaddr);
			port = ntohs(port) + port_adjust;
			new_server = 1;
			if (outfile) {
				fprintf(outfile, "%s %d.%d.%d.%d:%hu\n",
				    server_type ? server_type->type_string : "",
				    (ipaddr >> 24) & 0xff,
				    (ipaddr >> 16) & 0xff,
				    (ipaddr >> 8) & 0xff,
				    ipaddr & 0xff, port
				    );
			} else if (server_type == NULL) {
				xform_printf(OF, "%d.%d.%d.%d:%hu\n", (ipaddr >> 24) & 0xff, (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff, port);
			} else {
				add_qserver_byaddr(ipaddr, port, server_type, &new_server);
			}
			n_servers += new_server;
		}
		free(server->master_pkt);
		server->master_pkt = NULL;
		server->master_pkt_len = 0;
		server->n_servers = n_servers;
		if (outfile) {
			fclose(outfile);
		}
	}
	if (hostname_lookup) {
		hcache_update_file();
	}
	bind_sockets();
}


void
init_qserver(struct qserver *server, server_type *type)
{
	server->server_name = NULL;
	server->map_name = NULL;
	server->game = NULL;
	server->num_players = 0;
	server->num_spectators = 0;
	server->fd = -1;
	server->state = STATE_INIT;
	if (server->flags & FLAG_BROADCAST) {
		server->retry1 = 1;
		server->retry2 = 1;
	} else {
		server->retry1 = n_retries;
		server->retry2 = n_retries;
	}
	server->n_retries = 0;
	server->ping_total = 0;
	server->n_packets = 0;
	server->n_requests = 0;

	server->n_servers = 0;
	server->master_pkt_len = 0;
	server->master_pkt = NULL;
	server->error = NULL;

	server->saved_data.data = NULL;
	server->saved_data.datalen = 0;
	server->saved_data.pkt_index = -1;
	server->saved_data.pkt_max = 0;
	server->saved_data.next = NULL;

	server->type = type;
	server->next_rule = (get_server_rules) ? "" : NO_SERVER_RULES;
	server->next_player_info = (get_player_info && type->player_packet) ? 0 : NO_PLAYER_INFO;

	server->n_player_info = 0;
	server->players = NULL;
	server->n_rules = 0;
	server->rules = NULL;
	server->last_rule = &server->rules;
	server->missing_rules = 0;

	num_servers_total++;
}


// ipaddr should be network byte-order
// port should be host byte-order
// NOTE: This will return the first matching server, which is not nessacarily correct
// depending on if duplicate ports are allowed
struct qserver *
find_server_by_address(unsigned int ipaddr, unsigned short port)
{
	struct qserver **hashed;
	unsigned int hash, i;

	if (!noserverdups && show_errors) {
		fprintf(stderr, "error: find_server_by_address while duplicates are allowed, this is unsafe!");
	}

	hash = (ipaddr + port) % ADDRESS_HASH_LENGTH;

	if (ipaddr == 0) {
		for (hash = 0; hash < ADDRESS_HASH_LENGTH; hash++) {
			printf("%3d %d\n", hash, server_hash_len[hash]);
		}
		return (NULL);
	}

	hashed = server_hash[hash];
	for (i = server_hash_len[hash]; i; i--, hashed++) {
		if (*hashed && ((*hashed)->ipaddr == ipaddr) && ((*hashed)->port == port)) {
			return (*hashed);
		}
	}
	return (NULL);
}


void
add_server_to_hash(struct qserver *server)
{
	unsigned int hash;

	hash = (server->ipaddr + server->port) % ADDRESS_HASH_LENGTH;

	if (server_hash_len[hash] % 16 == 0) {
		server_hash[hash] = (struct qserver **)realloc(server_hash[hash], sizeof(struct qserver **) * (server_hash_len[hash] + 16));
		memset(&server_hash[hash][server_hash_len[hash]], 0, sizeof(struct qserver **) * 16);
	}
	server_hash[hash][server_hash_len[hash]] = server;
	server_hash_len[hash]++;
}


void
remove_server_from_hash(struct qserver *server)
{
	struct qserver **hashed;
	unsigned int hash, i, ipaddr = server->ipaddr;
	unsigned short port = server->orig_port;

	hash = (ipaddr + port) % ADDRESS_HASH_LENGTH;

	hashed = server_hash[hash];
	for (i = server_hash_len[hash]; i; i--, hashed++) {
		// NOTE: we use direct pointer checks here to prevent issues with duplicate port servers e.g. teamspeak 2 and 3
		if (*hashed == server) {
			*hashed = NULL;
			break;
		}
	}
}


void
free_server_hash()
{
	int i;

	for (i = 0; i < ADDRESS_HASH_LENGTH; i++) {
		if (server_hash[i]) {
			free(server_hash[i]);
		}
	}
}


/*
 * Functions for binding sockets to Quake servers
 */
int
bind_qserver_post(struct qserver *server)
{
	server->state = STATE_CONNECTED;

	if (server->type->flags & TF_TCP_CONNECT) {
		int one = 1;
		if (-1 == setsockopt(server->fd, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one))) {
			perror("Failed to set TCP no delay");
		}
	}

	if (server->type->id & MASTER_SERVER) {
		// Use a large buffer so we dont miss packets
		int sockbuf = RECV_BUF;
		if (-1 == setsockopt(server->fd, SOL_SOCKET, SO_RCVBUF, (void *)&sockbuf, sizeof(sockbuf))) {
			perror("Failed to set socket buffer");
		}
	}

#ifndef _WIN32
		if (server->fd >= max_connmap) {
			int old_max = max_connmap;
			max_connmap = server->fd + 32;
			connmap = (struct qserver **)realloc(connmap, max_connmap * sizeof(struct qserver *));
			memset(&connmap[old_max], 0, (max_connmap - old_max) * sizeof(struct qserver *));
		}
		connmap[server->fd] = server;
#endif
#ifdef _WIN32
		{
			int i;
			for (i = 0; i < max_connmap; i++) {
				if (connmap[i] == NULL) {
					connmap[i] = server;
					break;
				}
			}
			if (i >= max_connmap) {
				printf("could not put server in connmap\n");
			}
		}
#endif

	return (0);
}


void
add_ms_to_timeval(struct timeval *a, unsigned long interval_ms, struct timeval *result)
{
	result->tv_sec = a->tv_sec + (interval_ms / 1000);
	result->tv_usec = a->tv_usec + ((interval_ms % 1000) * 1000);
	if (result->tv_usec > 1000000) {
		result->tv_usec -= 1000000;
		result->tv_sec++;
	}
}


void
qserver_sockaddr(struct qserver *server, struct sockaddr_in *addr)
{
	addr->sin_family = AF_INET;
	addr->sin_port = (no_port_offset || server->flags & TF_NO_PORT_OFFSET) ?
	    htons(server->port) :
	    htons((unsigned short)(server->port + server->type->port_offset));
	addr->sin_addr.s_addr = server->ipaddr;
	memset(&(addr->sin_zero), 0, sizeof(addr->sin_zero));
}


int
connected_qserver(struct qserver *server, int polling)
{
	struct sockaddr_in addr;
	char error[50];
	int ret;
	struct timeval tv, now, to;
	fd_set connect_set;

	error[0] = '\0';
	gettimeofday(&now, NULL);
	add_ms_to_timeval(&server->packet_time1, retry_interval * server->retry1, &to);

	if (polling) {
		// No delay
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} else {
		// Wait until the server would timeout
		tv.tv_sec = to.tv_sec;
		tv.tv_usec = to.tv_usec;
	}

	while (1) {
		FD_ZERO(&connect_set);
		FD_SET(server->fd, &connect_set);

		// NOTE: We may need to check exceptfds here on windows instead of writefds
		ret = select(server->fd + 1, NULL, &connect_set, NULL, &tv);
		if (0 == ret) {
			// Time limit expired
			if (polling) {
				// Check if we have run out of time to connect
				gettimeofday(&now, NULL);
				if (0 < time_delta(&to, &now)) {
					// We still have time to connect
					return (-2);
				}
			}

			qserver_sockaddr(server, &addr);
			sprintf(error, "connect:%s:%u - timeout", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			server->server_name = TIMEOUT;
			server->state = STATE_TIMEOUT;
			goto connect_error;
		} else if (0 < ret) {
			// Socket selected for write so either connected or error
			int sockerr, orig_errno;
			unsigned int lon = sizeof(int);

			orig_errno = errno;
			if (0 != getsockopt(server->fd, SOL_SOCKET, SO_ERROR, (void *)(&sockerr), &lon)) {
				// Restore the original error
				errno = orig_errno;
				goto connect_error;
			}

			if (sockerr) {
				// set the real error
				errno = sockerr;
				goto connect_error;
			}

			// Connection success
			break;
		} else {
			// select failed
			if (errno != EINTR) {
				goto connect_error;
			}
		}
	}

	return (bind_qserver_post(server));

connect_error:
	if (STATE_CONNECTING == server->state) {
		// Default error case
		if (0 == strlen(error)) {
			qserver_sockaddr(server, &addr);
			sprintf(error, "connect: %s:%u", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		}
		server->server_name = SYSERROR;
		server->state = STATE_SYS_ERROR;
	}

	if (show_errors) {
		perror(error);
	}
	close(server->fd);
	server->fd = -1;
	connected--;

	return (-1);
}


int
bind_qserver2(struct qserver *server, int wait)
{
	struct sockaddr_in addr;
	static int one = 1;

	debug(1, "start %p @ %d.%d.%d.%d:%hu\n",
	    server,
	    server->ipaddr & 0xff,
	    (server->ipaddr >> 8) & 0xff,
	    (server->ipaddr >> 16) & 0xff,
	    (server->ipaddr >> 24) & 0xff,
	    server->port
	    );

	if (server->type->flags & TF_TCP_CONNECT) {
		server->fd = socket(AF_INET, SOCK_STREAM, 0);
	} else {
		server->fd = socket(AF_INET, SOCK_DGRAM, 0);
	}

	if (server->fd == INVALID_SOCKET) {
		if (sockerr() == EMFILE) {
			// Per process descriptor table is full - retry
			server->fd = -1;
			return (-2);
		}

		perror("socket");
		server->server_name = SYSERROR;
		server->state = STATE_SYS_ERROR;
		return (-1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(source_ip);
	if (server->type->id == Q2_MASTER) {
		addr.sin_port = htons(26500);
	} else if (source_port == 0) {
		addr.sin_port = 0;
	} else {
		addr.sin_port = htons(source_port);
		source_port++;
		if (source_port > source_port_high) {
			source_port = source_port_low;
		}
	}
	memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));

	if (bind(server->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
		perror("bind");
		server->server_name = SYSERROR;
		server->state = STATE_SYS_ERROR;
		close(server->fd);
		server->fd = -1;
		return (-1);
	}

	if (server->flags & FLAG_BROADCAST) {
		if (-1 == setsockopt(server->fd, SOL_SOCKET, SO_BROADCAST, (char *)&one, sizeof(one))) {
			perror("Failed to set broadcast");
			server->server_name = SYSERROR;
			server->state = STATE_SYS_ERROR;
			close(server->fd);
			server->fd = -1;
			return (-1);
		}
	}

	// we need nonblocking always. poll on an udp socket would wake
	// up and recv blocks if a packet with incorrect checksum is
	// received
	set_non_blocking(server->fd);

	if ((server->type->id != Q2_MASTER) && !(server->flags & FLAG_BROADCAST)) {
		if (server->type->flags & TF_TCP_CONNECT) {
			// TCP set packet_time1 so it can be used for ping calculations for protocols with an initial response
			gettimeofday(&server->packet_time1, NULL);
		}

		qserver_sockaddr(server, &addr);
		server->state = STATE_CONNECTING;

		if (connect(server->fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
			if (connection_inprogress()) {
				int ret;

				// Ensure we don't detect the same error twice, specifically on a different server
				clear_socketerror();

				if (!wait) {
					debug(2, "connect:%s:%u - in progress", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
					return (-3);
				}
				ret = connected_qserver(server, 0);
				if (0 != ret) {
					return (ret);
				}
			} else {
				if (show_errors) {
					char error[50];
					sprintf(error, "connect:%s:%u", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
					perror(error);
				}
				server->server_name = SYSERROR;
				server->state = STATE_SYS_ERROR;
				close(server->fd);
				server->fd = -1;
				return (-1);
			}
		}
	}

	return (bind_qserver_post(server));
}


int
bind_qserver(struct qserver *server)
{
	return (bind_qserver2(server, 1));
}


static struct timeval t_lastsend = { 0, 0 };

int
bind_sockets()
{
	struct qserver *server, *next_server, *first_server, *last_server;
	struct timeval now;
	int rc, retry_count = 0, inprogress = 0;

	gettimeofday(&now, NULL);
	if (connected && sendinterval && (time_delta(&now, &t_lastsend) < sendinterval)) {
		server = NULL;
	} else if (!waiting_for_masters) {
		if (last_server_bind == NULL) {
			last_server_bind = servers;
		}
		server = last_server_bind;
	} else {
		server = servers;
	}

	first_server = server;

	for ( ; server != NULL && connected < max_simultaneous; ) {
		// note the next server for use as process_func can free the server
		next_server = server->next;
		if ((server->server_name == NULL) && (server->fd == -1)) {
			if (waiting_for_masters && !server->type->master) {
				server = next_server;
				continue;
			}

			if ((rc = bind_qserver2(server, syncconnect ? 1 : 0)) == 0) {
				debug(1, "send %d.%d.%d.%d:%hu\n",
				    server->ipaddr & 0xff,
				    (server->ipaddr >> 8) & 0xff,
				    (server->ipaddr >> 16) & 0xff,
				    (server->ipaddr >> 24) & 0xff,
				    server->port
				    );

				gettimeofday(&t_lastsend, NULL);
				debug(2, "calling status_query_func for %p - connect", server);
				process_func_ret(server, server->type->status_query_func(server));

				connected++;
				if (!waiting_for_masters) {
					last_server_bind = server;
				}
				break;
			} else if (rc == -3) {
				// Connect in progress

				// We add to increment connected as we need to know the total
				// amount of connections in progress not just those that have
				// successfuly completed their connection otherwise we could
				// blow FD_SETSIZE
				connected++;
				inprogress++;
			} else if ((rc == -2) && (++retry_count > 2)) {
				return (-2);
			} else if (-1 == rc) {
				cleanup_qserver(server, FORCE);
			}
		}

		server = next_server;
	}

	// Wait for all connections to succeed or fail
	last_server = server;
	while (inprogress) {
		inprogress = 0;
		server = first_server;
		for ( ; server != last_server; ) {
			next_server = server->next;
			if (STATE_CONNECTING == server->state) {
				rc = connected_qserver(server, 1);
				switch (rc) {
				case 0:
					// Connected
					gettimeofday(&t_lastsend, NULL);
					debug(2, "calling status_query_func for %p - in progress", server);
					process_func_ret(server, server->type->status_query_func(server));

					// NOTE: connected is already incremented
					if (!waiting_for_masters) {
						last_server_bind = server;
					}
					break;

				case -2:
					// In progress
					inprogress++;
					break;

				case -1:
					cleanup_qserver(server, FORCE);
					break;
				}
			}
			server = next_server;
		}
	}

	if ((NULL != last_server) || (!connected && retry_count)) {
		// Retry later, more to process
		return (-2);
	}

	return (0);
}


int
process_func_ret(struct qserver *server, int ret)
{
	debug(3, "%p, %d", server, ret);
	switch (ret) {
	case INPROGRESS:
		return (ret);

	case DONE_AUTO:
		cleanup_qserver(server, NO_FORCE);
		return (ret);

	case DONE_FORCE:
	case SYS_ERROR:
	case MEM_ERROR:
	case PKT_ERROR:
	case ORD_ERROR:
	case REQ_ERROR:
		cleanup_qserver(server, FORCE);
		return (ret);
	}

	debug(0, "unhandled return code %d, please report!", ret);

	return (ret);
}


/*
 * Functions for sending packets
 */
// this is so broken, someone please rewrite the timeout handling
void
send_packets()
{
	struct qserver *server;
	struct timeval now;
	int interval, n_sent = 0, prev_n_sent;
	unsigned i;

	debug(3, "processing...");

	gettimeofday(&now, NULL);

	if (!t_lastsend.tv_sec) {
		// nothing
	} else if (connected && sendinterval && (time_delta(&now, &t_lastsend) < sendinterval)) {
		return;
	}

	for (i = 0; i < max_connmap; ++i) {
		server = connmap[i];
		if (!server) {
			continue;
		}

		if (server->fd == -1) {
			debug(0, "invalid entry in connmap\n");
		}

		if (server->type->id & MASTER_SERVER) {
			interval = master_retry_interval;
		} else {
			interval = retry_interval;
		}

		debug(2, "server %p, name %s, retry1 %d, next_rule %p, next_player_info %d, num_players %d, n_retries %d",
		    server,
		    server->server_name,
		    server->retry1,
		    server->next_rule,
		    server->next_player_info,
		    server->num_players,
		    n_retries
		    );
		prev_n_sent = n_sent;
		if (server->server_name == NULL) {
			// We havent seen the server yet?
			if ((server->retry1 != n_retries) && (time_delta(&now, &server->packet_time1) < (interval * (n_retries - server->retry1 + 1)))) {
				continue;
			}

			if (server->retry1 < 1) {
				// No more retries
				cleanup_qserver(server, FORCE);
				continue;
			}

			if ((qserver_get_timeout(server, &now) <= 0) && !(server->type->flags & TF_TCP_CONNECT)) {
				// Query status
				debug(2, "calling status_query_func for %p", server);
				process_func_ret(server, server->type->status_query_func(server));
				gettimeofday(&t_lastsend, NULL);
				n_sent++;
				continue;
			}
		}

		if (server->next_rule != NO_SERVER_RULES) {
			// We want server rules
			if ((server->retry1 != n_retries) && (time_delta(&now, &server->packet_time1) < (interval * (n_retries - server->retry1 + 1)))) {
				continue;
			}

			if (server->retry1 < 1) {
				// no more retries
				server->next_rule = NULL;
				server->missing_rules = 1;
				cleanup_qserver(server, NO_FORCE);
				continue;
			}
			debug(3, "send_rule_request_packet1");
			send_rule_request_packet(server);
			gettimeofday(&t_lastsend, NULL);
			n_sent++;
		}

		if (server->next_player_info < server->num_players) {
			// Expecting player details
			if ((server->retry2 != n_retries) && (time_delta(&now, &server->packet_time2) < (interval * (n_retries - server->retry2 + 1)))) {
				continue;
			}
			if (!server->retry2) {
				server->next_player_info++;
				if (server->next_player_info >= server->num_players) {
					// no more retries
					cleanup_qserver(server, FORCE);
					continue;
				}
				server->retry2 = n_retries;
			}
			send_player_request_packet(server);
			gettimeofday(&t_lastsend, NULL);
			n_sent++;
		}

		if (prev_n_sent == n_sent) {
			// we didnt send any additional queries
			debug(2, "no queries sent: %d %d", time_delta(&now, &server->packet_time1), (interval * (n_retries + 1)));
			if (server->retry1 < 1) {
				// no retries left
				if (time_delta(&now, &server->packet_time1) > (interval * (n_retries + 1))) {
					cleanup_qserver(server, FORCE);
				}
			} else {
				// decrement as we didnt send any packets
				server->retry1--;
			}
		}
	}

	debug(3, "done");
}


/* server starts sending data immediately, so we need not do anything */
query_status_t
send_bfris_request_packet(struct qserver *server)
{
	return (register_send(server));
}


/* First packet for a normal Quake server
 */
query_status_t
send_qserver_request_packet(struct qserver *server)
{
	return (send_packet(server, server->type->status_packet, server->type->status_len));
}


/* First packet for a QuakeWorld server
 */
query_status_t
send_qwserver_request_packet(struct qserver *server)
{
	int rc;

	if (server->flags & FLAG_BROADCAST) {
		rc = send_broadcast(server, server->type->status_packet, server->type->status_len);
	} else if (server->server_name == NULL) {
		rc = send(server->fd, server->type->status_packet, server->type->status_len, 0);
	} else if ((server->server_name != NULL) && server->type->rule_packet) {
		rc = send(server->fd, server->type->rule_packet, server->type->rule_len, 0);
	} else {
		rc = SOCKET_ERROR;
	}

	if (rc == SOCKET_ERROR) {
		return (send_error(server, rc));
	}

	if ((server->retry1 == n_retries) || server->flags & FLAG_BROADCAST) {
		gettimeofday(&server->packet_time1, NULL);
		server->n_requests++;
	} else if (server->server_name == NULL) {
		server->n_retries++;
	}
	server->retry1--;
	if (server->server_name == NULL) {
		server->n_packets++;
	}
	server->next_player_info = NO_PLAYER_INFO;      // we don't have a player packet

	return (INPROGRESS);
}


// First packet for an Unreal Tournament 2003 server
query_status_t
send_ut2003_request_packet(struct qserver *server)
{
	int ret = send_packet(server, server->type->status_packet, server->type->status_len);

	server->next_player_info = NO_PLAYER_INFO;

	return (ret);
}


// First packet for an Half-Life 2 server
query_status_t
send_hl2_request_packet(struct qserver *server)
{
	return (send_packet(server, server->type->status_packet, server->type->status_len));
}


/* First packet for an Unreal master
 */
query_status_t
send_unrealmaster_request_packet(struct qserver *server)
{
	return (send_packet(server, server->type->status_packet, server->type->status_len));
}


static const char *steam_region[] =
{
	"US East Coast", "US West Coast", "South America", "Europe", "Asia", "Australia", "Middle East", "Africa", NULL
};

char *
build_hlmaster_packet(struct qserver *server, int *len)
{
	static char packet[1600];
	char *pkt, *r, *sep = "";
	char *gamedir, *map, *flags;
	int flen;

	pkt = &packet[0];
	memcpy(pkt, server->type->master_packet, *len);

	if (server->type->flags & TF_MASTER_STEAM) {
		// default the region to 0xff
		const char *regionstring = get_param_value(server, "region", NULL);
		int region = 0xFF;
		if (regionstring) {
			char *tmp = NULL;
			region = strtol(regionstring, &tmp, 10);
			if (tmp == regionstring) {
				int i = 0;
				region = 0xFF;
				for ( ; steam_region[i]; ++i) {
					if (!strcmp(regionstring, steam_region[i])) {
						region = i;
						break;
					}
				}
			}
		}
		*(pkt + 1) = region;
	}

	pkt += *len;

	gamedir = get_param_value(server, "game", NULL);
	if (gamedir) {
		pkt += sprintf(pkt, "\\gamedir\\%s", gamedir);
	}

	// not valid for steam?
	map = get_param_value(server, "map", NULL);
	if (map) {
		pkt += sprintf(pkt, "\\map\\%s", map);
	}

	// steam
	flags = get_param_value(server, "napp", NULL);
	if (flags) {
		pkt += sprintf(pkt, "\\napp\\%s", flags);
	}

	// not valid for steam?
	flags = get_param_value(server, "status", NULL);
	r = flags;
	while (flags && sep) {
		sep = strchr(r, ':');
		if (sep) {
			flen = sep - r;
		} else {
			flen = strlen(r);
		}

		if (strncmp(r, "notempty", flen) == 0) {
			pkt += sprintf(pkt, "\\empty\\1");
		} else if (strncmp(r, "notfull", flen) == 0) {
			pkt += sprintf(pkt, "\\full\\1");
		} else if (strncmp(r, "dedicated", flen) == 0) {
			if (server->type->flags & TF_MASTER_STEAM) {
				pkt += sprintf(pkt, "\\type\\d");
			} else {
				pkt += sprintf(pkt, "\\dedicated\\1");
			}
		} else if (strncmp(r, "linux", flen) == 0) {
			pkt += sprintf(pkt, "\\linux\\1");
		} else if (strncmp(r, "proxy", flen) == 0) {
			// steam
			pkt += sprintf(pkt, "\\proxy\\1");
		} else if (strncmp(r, "secure", flen) == 0) {
			// steam
			pkt += sprintf(pkt, "\\secure\\1");
		}
		r = sep + 1;
	}

	// always need null terminator
	*pkt = 0x00;
	pkt++;

	*len = pkt - packet;

	return (packet);
}


/* First packet for a QuakeWorld master server
 */
query_status_t
send_qwmaster_request_packet(struct qserver *server)
{
	int rc = 0;

	server->next_player_info = NO_PLAYER_INFO;

	if (server->type->id == Q2_MASTER) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		if (no_port_offset || server->flags & TF_NO_PORT_OFFSET) {
			addr.sin_port = htons(server->port);
		} else {
			addr.sin_port = htons((unsigned short)(server->port + server->type->port_offset));
		}
		addr.sin_addr.s_addr = server->ipaddr;
		memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));
		rc = sendto(server->fd, server->type->master_packet, server->type->master_len, 0, (struct sockaddr *)&addr, sizeof(addr));
	} else {
		char *packet;
		int packet_len;
		char query_buf[4096] = { 0 };

		packet = server->type->master_packet;
		packet_len = server->type->master_len;

		if (server->type->id == HL_MASTER) {
			memcpy(server->type->master_packet + 1, server->master_query_tag, 3);
			if (server->query_arg) {
				packet_len = server->type->master_len;
				packet = build_hlmaster_packet(server, &packet_len);
			}
		} else if (server->type->flags & TF_MASTER_STEAM) {
			// region
			int tag_len = strlen(server->master_query_tag);
			if (tag_len < 9) {
				// initial case
				tag_len = 9;
				strcpy(server->master_query_tag, "0.0.0.0:0");
			}

			// 1 byte packet id
			// 1 byte region
			// ip:port
			// 1 byte null
			packet_len = 2 + tag_len + 1;

			if (server->query_arg) {
				// build_hlmaster_packet uses server->type->master_packet
				// as the basis so copy from server->master_query_tag
				strcpy(server->type->master_packet + 2, server->master_query_tag);
				packet = build_hlmaster_packet(server, &packet_len);
			} else {
				// default region
				*(packet + 1) = 0xff;
				memcpy(packet + 2, server->master_query_tag, tag_len);

				// filter null
				*(packet + packet_len) = 0x00;
				packet_len++;
			}
		} else if (server->type->flags & TF_QUERY_ARG) {
			// fill in the master protocol details
			char *master_protocol = server->query_arg;
			if (master_protocol == NULL) {
				master_protocol = server->type->master_protocol;
			}
			packet_len = sprintf(query_buf, server->type->master_packet,
				master_protocol ? master_protocol : "",
				server->type->master_query  ? server->type->master_query : ""
				);
			packet = query_buf;
		}

		rc = send(server->fd, packet, packet_len, 0);
	}

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


query_status_t
send_tribes_request_packet(struct qserver *server)
{
	return (send_packet(server, server->type->player_packet, server->type->player_len));
}


query_status_t
send_tribes2_request_packet(struct qserver *server)
{
	int rc;

	if (server->flags & FLAG_BROADCAST && (server->server_name == NULL)) {
		rc = send_broadcast(server, server->type->status_packet, server->type->status_len);
	} else if (server->server_name == NULL) {
		rc = send(server->fd, server->type->status_packet, server->type->status_len, 0);
	} else {
		rc = send(server->fd, server->type->player_packet, server->type->player_len, 0);
	}

	if (rc == SOCKET_ERROR) {
		return (send_error(server, rc));
	}

	register_send(server);

	return (rc);
}


query_status_t
send_savage_request_packet(struct qserver *server)
{
	int len;
	char *pkt;

	if (get_player_info) {
		pkt = server->type->player_packet;
		len = server->type->player_len;
	} else {
		pkt = server->type->status_packet;
		len = server->type->status_len;
	}

	return (send_packet(server, pkt, len));
}


query_status_t
send_farcry_request_packet(struct qserver *server)
{
	int len;
	char *pkt;

	if (get_player_info) {
		pkt = server->type->player_packet;
		len = server->type->player_len;
	} else {
		pkt = server->type->status_packet;
		len = server->type->status_len;
	}

	return (send_packet(server, pkt, len));
}


query_status_t
send_tribes2master_request_packet(struct qserver *server)
{
	int rc;
	unsigned char packet[1600], *pkt;
	unsigned int len, min_players, max_players, region_mask = 0;
	unsigned int build_version, max_bots, min_cpu, status;
	char *game, *mission, *buddies;
	static char *region_list[] =
	{
		"naeast", "nawest", "sa", "aus", "asia", "eur", NULL
	};
	static char *status_list[] =
	{
		"dedicated", "nopassword", "linux"
	};

	if (strcmp(get_param_value(server, "query", ""), "types") == 0) {
		rc = send(server->fd, tribes2_game_types_request, sizeof(tribes2_game_types_request), 0);
		goto send_done;
	}

	memcpy(packet, server->type->master_packet, server->type->master_len);

	pkt = packet + 7;

	game = get_param_value(server, "game", "any");
	len = strlen(game);
	if (len > 255) {
		len = 255;
	}
	*pkt++ = len;
	memcpy(pkt, game, len);
	pkt += len;

	mission = get_param_value(server, "mission", "any");
	len = strlen(mission);
	if (len > 255) {
		len = 255;
	}
	*pkt++ = len;
	memcpy(pkt, mission, len);
	pkt += len;

	min_players = get_param_ui_value(server, "minplayers", 0);
	max_players = get_param_ui_value(server, "maxplayers", 255);
	*pkt++ = min_players;
	*pkt++ = max_players;

	region_mask = get_param_ui_value(server, "regions", 0xffffffff);
	if (region_mask == 0) {
		char *regions = get_param_value(server, "regions", "");
		char *r = regions;
		char **list, *sep;
		do {
			sep = strchr(r, ':');
			if (sep) {
				len = sep - r;
			} else {
				len = strlen(r);
			}
			for (list = region_list; *list; list++) {
				if (strncasecmp(r, *list, len) == 0) {
					break;
				}
			}
			if (*list) {
				region_mask |= 1 << (list - region_list);
			}
			r = sep + 1;
		} while (sep);
	}
	if (little_endian) {
		memcpy(pkt, &region_mask, 4);
	} else {
		pkt[0] = region_mask & 0xff;
		pkt[1] = (region_mask >> 8) & 0xff;
		pkt[2] = (region_mask >> 16) & 0xff;
		pkt[3] = (region_mask >> 24) & 0xff;
	}
	pkt += 4;

	build_version = get_param_ui_value(server, "build", 0);

	/*
	 * if ( build_version && build_version < 22337) {
	 * packet[1]= 0;
	 * build_version= 0;
	 * }
	 */
	if (little_endian) {
		memcpy(pkt, &build_version, 4);
	} else {
		pkt[0] = build_version & 0xff;
		pkt[1] = (build_version >> 8) & 0xff;
		pkt[2] = (build_version >> 16) & 0xff;
		pkt[3] = (build_version >> 24) & 0xff;
	}
	pkt += 4;

	status = get_param_ui_value(server, "status", -1);
	if (status == 0) {
		char *flags = get_param_value(server, "status", "");
		char *r = flags;
		char **list, *sep;
		do {
			sep = strchr(r, ':');
			if (sep) {
				len = sep - r;
			} else {
				len = strlen(r);
			}
			for (list = status_list; *list; list++) {
				if (strncasecmp(r, *list, len) == 0) {
					break;
				}
			}
			if (*list) {
				status |= 1 << (list - status_list);
			}
			r = sep + 1;
		} while (sep);
	} else if (status == -1) {
		status = 0;
	}
	*pkt++ = status;

	max_bots = get_param_ui_value(server, "maxbots", 255);
	*pkt++ = max_bots;

	min_cpu = get_param_ui_value(server, "mincpu", 0);
	if (little_endian) {
		memcpy(pkt, &min_cpu, 2);
	} else {
		pkt[0] = min_cpu & 0xff;
		pkt[1] = (min_cpu >> 8) & 0xff;
	}
	pkt += 2;

	buddies = get_param_value(server, "buddies", NULL);
	if (buddies) {
		char *b = buddies, *sep;
		unsigned int buddy, n_buddies = 0;
		unsigned char *n_loc = pkt++;
		do {
			sep = strchr(b, ':');
			if (sscanf(b, "%u", &buddy)) {
				n_buddies++;
				if (little_endian) {
					memcpy(pkt, &buddy, 4);
				} else {
					pkt[0] = buddy & 0xff;
					pkt[1] = (buddy >> 8) & 0xff;
					pkt[2] = (buddy >> 16) & 0xff;
					pkt[3] = (buddy >> 24) & 0xff;
				}
				pkt += 4;
			}
			b = sep + 1;
		} while (sep && n_buddies < 255);
		*n_loc = n_buddies;
	} else {
		*pkt++ = 0;
	}

	rc = send(server->fd, (char *)packet, pkt - packet, 0);

send_done:
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


static struct _gamespy_query_map {
	char *qstat_type;
	char *gamespy_type;
}

gamespy_query_map[] =
{
	{ "qws", "quakeworld" },
	{ "q2s", "quake2"     },
	{ "q3s", "quake3"     },
	{ "tbs", "tribes"     },
	{ "uns", "ut"	      },
	{ "sgs", "shogo"      },
	{ "hls", "halflife"   },
	{ "kps", "kingpin"    },
	{ "hrs", "heretic2"   },
	{ "sfs", "sofretail"  },
	{ NULL,	 NULL	      }
};

query_status_t
send_gamespy_master_request(struct qserver *server)
{
	int rc, i;
	char request[1024];

	if (server->n_packets) {
		return (DONE_AUTO);
	}

	// WARNING: This only works for masters which don't check the value of validate
	// e.g. unreal.epicgames.com
	//
	// What we should be doing here is recieving the challenge from the master
	// processing the secure key and then using that as the value for validate.
	//
	// The details of this can be seen in gslist:
	// http://aluigi.altervista.org/papers.htm#gslist
	rc = send(server->fd, server->type->master_packet, server->type->master_len, 0);
	if (rc != server->type->master_len) {
		return (send_error(server, rc));
	}

	strcpy(request, server->type->status_packet);

	for (i = 0; gamespy_query_map[i].qstat_type; i++) {
		if (strcasecmp(server->query_arg, gamespy_query_map[i].qstat_type) == 0) {
			break;
		}
	}

	if (gamespy_query_map[i].gamespy_type == NULL) {
		strcat(request, server->query_arg);
	} else {
		strcat(request, gamespy_query_map[i].gamespy_type);
	}
	strcat(request, "\\final\\");
	assert(strlen(request) < sizeof(request));

	rc = send(server->fd, request, strlen(request), 0);
	if (rc != strlen(request)) {
		return (send_error(server, rc));
	}

	if (server->retry1 == n_retries) {
		gettimeofday(&server->packet_time1, NULL);
		server->n_requests++;
	}

	server->n_packets++;

	return (INPROGRESS);
}


query_status_t
send_rule_request_packet(struct qserver *server)
{
	int rc, len;

	debug(3, "send_rule_request_packet: %p", server);

	/* Server created via broadcast, so bind it */
	if (server->fd == -1) {
		if (bind_qserver(server) < 0) {
			goto setup_retry;
		}
	}

	if (server->type->rule_query_func && (server->type->rule_query_func != send_rule_request_packet)) {
		return (server->type->rule_query_func(server));
	}

	if (server->type->id == Q_SERVER) {
		strcpy((char *)q_rule.data, server->next_rule);
		len = Q_HEADER_LEN + strlen((char *)q_rule.data) + 1;
		q_rule.length = htons((short)len);
	} else {
		len = server->type->rule_len;
	}

	rc = send(server->fd, (const char *)server->type->rule_packet, len, 0);
	if (rc == SOCKET_ERROR) {
		return (send_error(server, rc));
	}

setup_retry:
	if (server->retry1 == n_retries) {
		gettimeofday(&server->packet_time1, NULL);
		server->n_requests++;
	} else if (server->server_name == NULL) {
		server->n_retries++;
	}
	server->retry1--;
	if (server->server_name == NULL) {
		server->n_packets++;
	}

	return (DONE_AUTO);
}


query_status_t
send_player_request_packet(struct qserver *server)
{
	int rc;

	debug(3, "send_player_request_packet %p", server);

	if (!server->type->player_packet) {
		return (0);
	}

	/* Server created via broadcast, so bind it */
	if (server->fd == -1) {
		if (bind_qserver(server) < 0) {
			goto setup_retry;
		}
	}

	if (server->type->player_query_func && (server->type->player_query_func != send_player_request_packet)) {
		return (server->type->player_query_func(server));
	}

	if (!server->type->player_packet) {
		debug(0, "error: server %p has no player_packet", server);
		return (0);
	}

	if (server->type->id == Q_SERVER) {
		q_player.data[0] = server->next_player_info;
	}
	rc = send(server->fd, (const char *)server->type->player_packet, server->type->player_len, 0);
	if (rc == SOCKET_ERROR) {
		return (send_error(server, rc));
	}

setup_retry:
	if (server->retry2 == n_retries) {
		gettimeofday(&server->packet_time2, NULL);
		server->n_requests++;
	} else {
		server->n_retries++;
	}
	server->retry2--;
	server->n_packets++;

	return (1);
}


void
qserver_disconnect(struct qserver *server)
{
#ifdef _WIN32
		int i;
#endif
	if (server->fd != -1) {
		close(server->fd);
#ifndef _WIN32
			connmap[server->fd] = NULL;
#else
			for (i = 0; i < max_connmap; i++) {
				if (connmap[i] == server) {
					connmap[i] = NULL;
					break;
				}
			}
#endif
		server->fd = -1;
		connected--;
	}
}


/* Functions for figuring timeouts and when to give up
 * Returns 1 if the query is done (server may be freed) and 0 if not.
 */
int
cleanup_qserver(struct qserver *server, int force)
{
	int close_it = force;

	debug(3, "cleanup_qserver %p, %d", server, force);
	if (server->server_name == NULL) {
		debug(3, "server has no name, forcing close");
		close_it = 1;
		if (server->type->id & MASTER_SERVER && (server->master_pkt != NULL)) {
			server->server_name = MASTER;
		} else {
			server->server_name = TIMEOUT;
			num_servers_timed_out++;
		}
	} else if (server->type->flags & TF_SINGLE_QUERY) {
		debug(3, "TF_SINGLE_QUERY, forcing close");
		close_it = 1;
	} else if ((server->next_rule == NO_SERVER_RULES) && (server->next_player_info >= server->num_players)) {
		debug(3, "no server rules and next_player_info >= num_players, forcing close");
		close_it = 1;
	}

	debug(3, "close_it %d", close_it);
	if (close_it) {
		if (server->saved_data.data) {
			SavedData *sdata = server->saved_data.next;
			free(server->saved_data.data);
			server->saved_data.data = NULL;
			while (sdata != NULL) {
				SavedData *next;
				free(sdata->data);
				next = sdata->next;
				free(sdata);
				sdata = next;
			}
			server->saved_data.next = NULL;
		}

		qserver_disconnect(server);

		if (server->server_name != TIMEOUT) {
			num_servers_returned++;
			if (server->server_name != DOWN) {
				num_players_total += server->num_players;
				max_players_total += server->max_players;
			}
		}
		if ((server->server_name == TIMEOUT) || (server->server_name == DOWN)) {
			server->ping_total = 999999;
		}
		if (server->type->master) {
			waiting_for_masters--;
			if (waiting_for_masters == 0) {
				add_servers_from_masters();
			}
		}
		if (!server_sort) {
			display_server(server);
		}
		return (1);
	}
	return (0);
}


/** must be called only on connected servers
 * @returns time in ms until server needs timeout handling. timeout handling is needed if <= zero
 */
static int
qserver_get_timeout(struct qserver *server, struct timeval *now)
{
	int diff, diff1, diff2, interval;

	if (server->type->id & MASTER_SERVER) {
		interval = master_retry_interval;
	} else {
		interval = retry_interval;
	}

	diff2 = 0xffff;

	diff1 = interval * (n_retries - server->retry1 + 1) - time_delta(now, &server->packet_time1);

	if (server->next_player_info < server->num_players) {
		diff2 = interval * (n_retries - server->retry2 + 1) - time_delta(now, &server->packet_time2);
	}

	debug(2, "timeout for %p is diff1 %d diff2 %d", server, diff1, diff2);

	diff = (diff1 < diff2) ? diff1 : diff2;

	return (diff);
}


void
get_next_timeout(struct timeval *timeout)
{
	struct qserver *server = servers;
	struct timeval now;
	int diff, smallest = retry_interval + master_retry_interval;
	int bind_count = 0;

	if (first_server_bind == NULL) {
		first_server_bind = servers;
	}

	server = first_server_bind;

	for ( ; server != NULL && server->fd == -1; server = server->next) {
	}

	/* if there are unconnected servers and slots left we retry in 10ms */
	if ((server == NULL) || ((num_servers > connected) && (connected < max_simultaneous))) {
		timeout->tv_sec = 0;
		timeout->tv_usec = 10 * 1000;
		return;
	}

	first_server_bind = server;

	gettimeofday(&now, NULL);
	for ( ; server != NULL && bind_count < connected; server = server->next) {
		if (server->fd == -1) {
			continue;
		}

		diff = qserver_get_timeout(server, &now);
		if (diff < smallest) {
			smallest = diff;
		}
		bind_count++;
	}

	if (smallest < 10) {
		smallest = 10;
	}

	timeout->tv_sec = smallest / 1000;
	timeout->tv_usec = (smallest % 1000) * 1000;
}


#ifdef USE_SELECT
	static fd_set select_read_fds;
	static int select_maxfd;
	static int select_cursor;

	int
	set_fds(fd_set *fds)
	{
		int maxfd = -1, i;

		for (i = 0; i < max_connmap; i++) {
			if (connmap[i] != NULL) {
				FD_SET(connmap[i]->fd, fds);
				if (connmap[i]->fd > maxfd) {
					maxfd = connmap[i]->fd;
				}
			}
		}

		return (maxfd);
	}


	void
	set_file_descriptors()
	{
		FD_ZERO(&select_read_fds);
		select_maxfd = set_fds(&select_read_fds);
	}


	int
	wait_for_file_descriptors(struct timeval *timeout)
	{
		select_cursor = 0;
		return (select(select_maxfd + 1, &select_read_fds, NULL, NULL, timeout));
	}


	struct qserver *
	get_next_ready_server()
	{
		while (select_cursor < max_connmap && (connmap[select_cursor] == NULL || !FD_ISSET(connmap[select_cursor]->fd, &select_read_fds))) {
			select_cursor++;
		}

		if (select_cursor >= max_connmap) {
			return (NULL);
		}
		return (connmap[select_cursor++]);
	}


	int
	wait_for_timeout(unsigned int ms)
	{
		struct timeval timeout;

		timeout.tv_sec = ms / 1000;
		timeout.tv_usec = (ms % 1000) * 1000;
		return (select(0, 0, NULL, NULL, &timeout));
	}


#endif  /* USE_SELECT */

#ifdef USE_POLL
	static struct pollfd *pollfds;
	static int n_pollfds;
	static int max_pollfds = 0;
	static int poll_cursor;

	void
	set_file_descriptors()
	{
		struct pollfd *p;
		int i;

		if (max_connmap > max_pollfds) {
			max_pollfds = max_connmap;
			pollfds = (struct pollfd *)realloc(pollfds, max_pollfds * sizeof(struct pollfd));
		}

		p = pollfds;
		for (i = 0; i < max_connmap; i++) {
			if (connmap[i] != NULL) {
				p->fd = connmap[i]->fd;
				p->events = POLLIN;
				p->revents = 0;
				p++;
			}
		}
		n_pollfds = p - pollfds;
	}


	int
	wait_for_file_descriptors(struct timeval *timeout)
	{
		poll_cursor = 0;
		return (poll(pollfds, n_pollfds, timeout->tv_sec * 1000 + timeout->tv_usec / 1000));
	}


	struct qserver *
	get_next_ready_server()
	{
		for ( ; poll_cursor < n_pollfds; poll_cursor++) {
			if (pollfds[poll_cursor].revents) {
				break;
			}
		}

		if (poll_cursor >= n_pollfds) {
			return (NULL);
		}
		return (connmap[pollfds[poll_cursor++].fd]);
	}


	int
	wait_for_timeout(unsigned int ms)
	{
		return (poll(0, 0, ms));
	}


#endif  /* USE_POLL */

void
free_server(struct qserver *server)
{
	struct player *player, *next_player;
	struct rule *rule, *next_rule;

	/* remove from servers list */
	if (server == servers) {
		servers = server->next;
		if (servers) {
			servers->prev = NULL;
		}
	}

	if ((void *)&server->next == (void *)last_server) {
		if (server->prev) {
			last_server = &server->prev->next;
		} else {
			last_server = &servers;
		}
	}
	if (server == first_server_bind) {
		first_server_bind = server->next;
	}
	if (server == last_server_bind) {
		last_server_bind = server->next;
	}

	if (server->prev) {
		server->prev->next = server->next;
	}
	if (server->next) {
		server->next->prev = server->prev;
	}

	/* remove from server hash table */
	remove_server_from_hash(server);

	/* free all the data */
	for (player = server->players; player; player = next_player) {
		next_player = player->next;
		free_player(player);
	}

	for (rule = server->rules; rule; rule = next_rule) {
		next_rule = rule->next;
		free_rule(rule);
	}

	if (server->arg) {
		free(server->arg);
	}
	if (server->host_name) {
		free(server->host_name);
	}
	if (server->error) {
		free(server->error);
	}
	if (server->address) {
		free(server->address);
	}
	if (server->map_name) {
		free(server->map_name);
	}
	if (!(server->flags & FLAG_DO_NOT_FREE_GAME) && server->game) {
		free(server->game);
	}
	if (server->master_pkt) {
		free(server->master_pkt);
	}
	if (server->query_arg) {
		free(server->query_arg);
	}
	if (server->challenge_string) {
		free(server->challenge_string);
	}

	/* These fields are never malloc'd: outfilename
	 */

	if (
		(server->server_name != NULL) &&
		(server->server_name != DOWN) &&
		(server->server_name != HOSTNOTFOUND) &&
		(server->server_name != SYSERROR) &&
		(server->server_name != MASTER) &&
		(server->server_name != SERVERERROR) &&
		(server->server_name != TIMEOUT) &&
		(server->server_name != GAMESPY_MASTER_NAME) &&
		(server->server_name != BFRIS_SERVER_NAME)
		) {
		free(server->server_name);
	}

	/*
	 * params ...
	 * saved_data ...
	 */

	free(server);
	--num_servers;
}


void
free_player(struct player *player)
{
	if (player->name) {
		free(player->name);
	}
	if (!(player->flags & PLAYER_FLAG_DO_NOT_FREE_TEAM) && player->team_name) {
		free(player->team_name);
	}
	if (player->address) {
		free(player->address);
	}
	if (player->tribe_tag) {
		free(player->tribe_tag);
	}
	if (player->skin) {
		free(player->skin);
	}
	if (player->mesh) {
		free(player->mesh);
	}
	if (player->face) {
		free(player->face);
	}
	free(player);
}


void
free_rule(struct rule *rule)
{
	if (rule->name) {
		free(rule->name);
	}

	if (rule->value) {
		free(rule->value);
	}
	free(rule);
}


/* Functions for handling response packets
 */

/* Packet from normal Quake server
 */
query_status_t
deal_with_q_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	struct q_packet *pkt = (struct q_packet *)rawpkt;
	int rc;

	debug(2, "deal_with_q_packet %p, %d", server, pktlen);

	if (ntohs(pkt->length) != pktlen) {
		fprintf(stderr, "%s Ignoring bogus packet; length %d != %d\n", server->arg, ntohs(pkt->length), pktlen);
		return (PKT_ERROR);
	}

	rawpkt[pktlen] = '\0';

	switch (pkt->op_code) {
	case Q_CCREP_ACCEPT:
	case Q_CCREP_REJECT:
		return (0);

	case Q_CCREP_SERVER_INFO:
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		rc = server_info_packet(server, pkt, pktlen - Q_HEADER_LEN);
		break;

	case Q_CCREP_PLAYER_INFO:
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time2);
		rc = player_info_packet(server, pkt, pktlen - Q_HEADER_LEN);
		break;

	case Q_CCREP_RULE_INFO:
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		rc = rule_info_packet(server, pkt, pktlen - Q_HEADER_LEN);
		break;

	case Q_CCREQ_CONNECT:
	case Q_CCREQ_SERVER_INFO:
	case Q_CCREQ_PLAYER_INFO:
	case Q_CCREQ_RULE_INFO:
	default:
		return (0);
	}

	if (SOCKET_ERROR == rc) {
		fprintf(stderr, "%s error on packet opcode %x\n", server->arg, (int)pkt->op_code);
	}

	return (rc);
}


/* Packet from QuakeWorld server
 */
query_status_t
deal_with_qw_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	debug(2, "deal_with_qw_packet %p, %d", server, pktlen);
	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	}

	if ((((rawpkt[0] != '\377') && (rawpkt[0] != '\376')) || (rawpkt[1] != '\377') || (rawpkt[2] != '\377') || (rawpkt[3] != '\377')) && show_errors) {
		unsigned int ipaddr = ntohl(server->ipaddr);
		fprintf(stderr, "Odd packet from server %d.%d.%d.%d:%hu, processing ...\n",
		    (ipaddr >> 24) & 0xff,
		    (ipaddr >> 16) & 0xff,
		    (ipaddr >> 8) & 0xff,
		    ipaddr & 0xff,
		    ntohs(server->port)
		    );
		print_packet(server, rawpkt, pktlen);
	}

	rawpkt[pktlen] = '\0';

	if (rawpkt[4] == 'n') {
		if (server->type->id != QW_SERVER) {
			server->type = find_server_type_id(QW_SERVER);
		}
		return (deal_with_q1qw_packet(server, rawpkt, pktlen));
	} else if ((rawpkt[4] == '\377') && (rawpkt[5] == 'n')) {
		if (server->type->id != HW_SERVER) {
			server->type = find_server_type_id(HW_SERVER);
		}
		return (deal_with_q1qw_packet(server, rawpkt, pktlen));
	} else if (strncmp(&rawpkt[4], "print\n\\", 7) == 0) {
		return (deal_with_q2_packet(server, rawpkt + 10, pktlen - 10));
	} else if (strncmp(&rawpkt[4], "print\n", 6) == 0) {
		/* work-around for occasional bug in Quake II status packets
		 */
		char *c, *p;
		p = c = &rawpkt[10];
		while (*p != '\\' && (c = strchr(p, '\n'))) {
			p = c + 1;
		}
		if ((*p == '\\') && (c != NULL)) {
			return (deal_with_q2_packet(server, p, pktlen - (p - rawpkt)));
		}
	} else if ((strncmp(&rawpkt[4], "infoResponse", 12) == 0) || ((rawpkt[4] == '\001') && (strncmp(&rawpkt[5], "infoResponse", 12) == 0))) {
		/* quake3 info response */
		int ret;
		if (rawpkt[4] == '\001') {
			rawpkt++;
			pktlen--;
		}
		rawpkt += 12;
		pktlen -= 12;
		for ( ; pktlen && *rawpkt != '\\'; pktlen--, rawpkt++) {
		}
		if (!pktlen) {
			return (INPROGRESS);
		}
		if (rawpkt[pktlen - 1] == '"') {
			rawpkt[pktlen - 1] = '\0';
			pktlen--;
		}
		if (get_player_info || get_server_rules) {
			server->next_rule = "";
		}

		ret = deal_with_q2_packet(server, rawpkt, pktlen);
		if ((DONE_AUTO == ret) && (get_player_info || get_server_rules)) {
			debug(3, "send_rule_request_packet2");
			send_rule_request_packet(server);
			server->retry1 = n_retries - 1;
			return (INPROGRESS);
		}

		return (ret);
	} else if ((strncmp(&rawpkt[4], "statusResponse\n", 15) == 0) || ((rawpkt[4] == '\001') && (strncmp(&rawpkt[5], "statusResponse\n", 15) == 0))) {
		/* quake3 status response */
		server->next_rule = NO_SERVER_RULES;
		server->retry1 = 0;
		if (rawpkt[4] == '\001') {
			rawpkt++;
			pktlen--;
		}
		server->flags |= CHECK_DUPLICATE_RULES;
		return (deal_with_q2_packet(server, rawpkt + 19, pktlen - 19));
	} else if (strncmp(&rawpkt[4], "infostringresponse", 19) == 0) {
		return (deal_with_q2_packet(server, rawpkt + 23, pktlen - 23));
	}

	if (show_errors) {
		unsigned int ipaddr = ntohl(server->ipaddr);
		fprintf(stderr, "Odd packet from server %d.%d.%d.%d:%hu, ignoring ...\n",
		    (ipaddr >> 24) & 0xff,
		    (ipaddr >> 16) & 0xff,
		    (ipaddr >>  8) & 0xff,
		    ipaddr & 0xff,
		    ntohs(server->port)
		    );
		print_packet(server, rawpkt, pktlen);
		return (PKT_ERROR);
	}

	return (DONE_AUTO);
}


query_status_t
deal_with_q1qw_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *key, *value, *end, *users;
	struct player *player = NULL, **last_player = &server->players;
	int len, rc, complete = 0;
	int number, frags, connect_time, ping;
	char *pkt = &rawpkt[5];

	debug(2, "deal_with_q1qw_packet %p, %d", server, pktlen);

	if (server->type->id == HW_SERVER) {
		pkt = &rawpkt[6];
	}

	*(users = strchr(pkt, '\n')) = '\0';
	while (*pkt && pkt - rawpkt < pktlen) {
		if (*pkt == '\\') {
			pkt++;
			end = strchr(pkt, '\\');
			if (end == NULL) {
				break;
			}
			*end = '\0';
			key = pkt;
			pkt += strlen(pkt) + 1;
			end = strchr(pkt, '\\');
			if (end == NULL) {
				end = users;
			}
			value = (char *)malloc(end - pkt + 1);
			memcpy(value, pkt, end - pkt);
			value[end - pkt] = '\0';
			pkt = end;
			if (strcmp(key, "hostname") == 0) {
				server->server_name = value;
			} else if (strcmp(key, "map") == 0) {
				server->map_name = value;
			} else if (strcmp(key, "maxclients") == 0) {
				server->max_players = atoi(value);
				free(value);
			} else if (strcmp(key, "maxspectators") == 0) {
				server->max_spectators = atoi(value);
				free(value);
			} else if (get_server_rules || (strncmp(key, "*game", 5) == 0)) {
				add_rule(server, key, value, NO_VALUE_COPY);
				if (strcmp(key, "*gamedir") == 0) {
					server->game = value;
					server->flags |= FLAG_DO_NOT_FREE_GAME;
				}
			}
		} else {
			pkt++;
		}
		complete = 1;
	}
	*pkt = '\n';
	while (*pkt && pkt - rawpkt < pktlen) {
		if (*pkt == '\n') {
			pkt++;
			if ((pkt - rawpkt >= pktlen) || (*pkt == '\0')) {
				break;
			}
			rc = sscanf(pkt, "%d %d %d %d %n", &number, &frags, &connect_time, &ping, &len);
			if (rc != 4) {
				char *nl;       /* assume it's an error packet */
				server->error = (char *)malloc(pktlen + 1);
				nl = strchr(pkt, '\n');
				if (nl != NULL) {
					strncpy(server->error, pkt, nl - pkt);
					server->error[nl - pkt] = '\0';
				} else {
					strcpy(server->error, pkt);
				}
				server->server_name = SERVERERROR;
				complete = 1;
				break;
			}
			if (get_player_info) {
				player = (struct player *)calloc(1, sizeof(struct player));
				player->number = number;
				player->frags = frags;
				player->connect_time = connect_time * 60;
				player->ping = ping > 0 ? ping : -ping;
			} else {
				player = NULL;
			}

			pkt += len;

			if (*pkt != '"') {
				break;
			}
			pkt += ping > 0 ? 1 : 4;// if 4 then no "\s\" in spectators name
			// protocol "under construction"
			end = strchr(pkt, '"');
			if (end == NULL) {
				break;
			}
			if (player != NULL) {
				player->name = (char *)malloc(end - pkt + 1);
				memcpy(player->name, pkt, end - pkt);
				player->name[end - pkt] = '\0';
			}

			pkt = end + 2;

			if (*pkt != '"') {
				break;
			}
			pkt++;
			end = strchr(pkt, '"');
			if (end == NULL) {
				break;
			}
			if (player != NULL) {
				player->skin = (char *)malloc(end - pkt + 1);
				memcpy(player->skin, pkt, end - pkt);
				player->skin[end - pkt] = '\0';
			}
			pkt = end + 2;

			if (player != NULL) {
				sscanf(pkt, "%d %d%n", &player->shirt_color, &player->pants_color, &len);
				*last_player = player;
				last_player = &player->next;
			} else {
				sscanf(pkt, "%*d %*d%n", &len);
			}
			pkt += len;

			if ((pkt + 3 < rawpkt + pktlen) && (*pkt == ' ')) {
				// mvdsv is at last rev 377, 23.06.2006
				pkt++;

				if (*pkt != '"') {
					break;
				}
				pkt++;
				end = strchr(pkt, '"');
				if (end == NULL) {
					break;
				}

				if (player != NULL) {
					player->team_name = (char *)malloc(end - pkt + 1);
					memcpy(player->team_name, pkt, end - pkt);
					player->team_name[end - pkt] = '\0';
				}
				pkt = end + 1;
			}

			if (ping > 0) {
				server->num_players++;
			} else {
				server->num_spectators++;
			}
		} else {
			pkt++;
		}
		complete = 1;
	}

	if (!complete) {
		if ((rawpkt[4] != 'n') || (rawpkt[5] != '\0')) {
			fprintf(stderr, "Odd packet from QW server %d.%d.%d.%d:%hu ...\n",
			    (server->ipaddr >> 24) & 0xff,
			    (server->ipaddr >> 16) & 0xff,
			    (server->ipaddr >> 8) & 0xff,
			    server->ipaddr & 0xff,
			    ntohs(server->port)
			    );
			print_packet(server, rawpkt, pktlen);
		}
	} else if (server->server_name == NULL) {
		server->server_name = strdup("");
	}

	return (DONE_AUTO);
}


query_status_t
deal_with_q2_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *key, *value, *end;
	struct player *player = NULL;
	struct player **last_player = &server->players;
	int len, rc, complete = 0;
	int frags = 0, ping = 0, num_players = 0;
	char *pkt = rawpkt;

	debug(2, "deal_with_q2_packet %p, %d", server, pktlen);

	while (*pkt && pkt - rawpkt < pktlen) {
		// we have variable, value pairs seperated by slash
		if (*pkt == '\\') {
			pkt++;
			if ((*pkt == '\n') && (server->type->id == SOF_SERVER)) {
				goto player_info;
			}

			// Find the key
			end = strchr(pkt, '\\');
			if (NULL == end) {
				break;
			}
			*end = '\0';
			key = pkt;
			pkt += strlen(key) + 1;

			// Find the value
			end = strpbrk(pkt, "\\\n");
			if (NULL == end) {
				// Last value
				end = rawpkt + pktlen;
			}

			// Make a copy of the value
			value = (char *)malloc(end - pkt + 1);
			memcpy(value, pkt, end - pkt);
			value[end - pkt] = '\0';
			pkt = end;
			debug(3, "%s = %s", key, value);
			if ((server->server_name == NULL) && ((strcmp(key, "hostname") == 0) || (strcmp(key, "sv_hostname") == 0))) {
				// Server name
				server->server_name = value;
			} else if ((strcmp(key, "mapname") == 0) || (strcmp(key, "map_name") == 0) || ((strcmp(key, "map") == 0) && (server->map_name == NULL))) {
				// Map name
				if (NULL != server->map_name) {
					free(server->map_name);
				}
				server->map_name = value;
			} else if ((strcmp(key, "maxclients") == 0) || (strcmp(key, "sv_maxclients") == 0) || (strcmp(key, "sv_max_clients") == 0) || (strcmp(key, "max") == 0)) {
				// Max Players
				server->max_players = atoi(value);
				// Note: COD 4 infoResponse returns max as sv_maxclients - sv_privateClients where as statusResponse returns the true max
				// MOHAA Q3 protocol max players is always 0
				if (0 == server->max_players) {
					server->max_players = -1;
				}
				free(value);
			} else if ((strcmp(key, "clients") == 0) || (strcmp(key, "players") == 0)) {
				// Num Players
				server->num_players = atoi(value);
				free(value);
			} else if ((server->server_name == NULL) && (strcmp(key, "pure") == 0)) {
				add_rule(server, key, value, NO_VALUE_COPY);
			} else if (get_server_rules || (strncmp(key, "game", 4) == 0)) {
				int dofree = 0;
				int flags = (server->flags & CHECK_DUPLICATE_RULES) ? CHECK_DUPLICATE_RULES | NO_VALUE_COPY : NO_VALUE_COPY;
				if (add_rule(server, key, value, flags) == NULL) {
					// duplicate, so free value
					dofree = 1;
				}

				if ((server->game == NULL) && (strcmp(key, server->type->game_rule) == 0)) {
					server->game = value;
					if (0 == dofree) {
						server->flags |= FLAG_DO_NOT_FREE_GAME;
					}
				} else if (1 == dofree) {
					free(value);
				}
			} else {
				free(value);
			}
		} else if (*pkt == '\n') {
player_info:            debug(3, "player info");
			pkt++;
			if (*pkt == '\0') {
				break;
			}

			if (0 == strncmp(pkt, "\\challenge\\", 11)) {
				// qfusion
				// This doesnt support getstatus looking at warsow source:
				// server/sv_main.c: SV_ConnectionlessPacket
				server->next_rule = NO_SERVER_RULES;
				debug(3, "no more server rules");
				break;
			}

			rc = sscanf(pkt, "%d %n", &frags, &len);
			if ((rc == 1) && (pkt[len] != '"')) {
				pkt += len;
				rc = sscanf(pkt, "%d %n", &ping, &len);
			} else if (rc == 1) {
				/* MOHAA Q3 protocol only provides player ping */
				ping = frags;
				frags = 0;
			}

			if (rc != 1) {
				char *nl;       /* assume it's an error packet */
				server->error = (char *)malloc(pktlen + 1);
				nl = strchr(pkt, '\n');
				if (nl != NULL) {
					strncpy(server->error, pkt, nl - pkt);
				} else {
					strcpy(server->error, pkt);
				}
				server->server_name = SERVERERROR;
				complete = 1;
				break;
			}

			if (get_player_info) {
				player = (struct player *)calloc(1, sizeof(struct player));
				player->number = 0;
				player->connect_time = -1;
				player->frags = frags;
				player->ping = ping;
			} else {
				player = NULL;
			}

			pkt += len;

			if (isdigit((unsigned char)*pkt)) {
				/* probably an SOF2 1.01 server, includes team # */
				int team;
				rc = sscanf(pkt, "%d %n", &team, &len);
				if (rc == 1) {
					pkt += len;
					if (player) {
						player->team = team;
						server->flags |= FLAG_PLAYER_TEAMS;
					}
				}
			}

			if (*pkt != '"') {
				break;
			}

			pkt++;
			end = strchr(pkt, '"');
			if (end == NULL) {
				break;
			}
			if (player != NULL) {
				player->name = (char *)malloc(end - pkt + 1);
				memcpy(player->name, pkt, end - pkt);
				player->name[end - pkt] = '\0';
			}
			pkt = end + 1;

			//WarSoW team number
			if (*pkt != '\n') {
				int team;
				rc = sscanf(pkt, "%d%n", &team, &len);
				if (rc == 1) {
					pkt += len;
					if (player) {
						player->team = team;
						server->flags |= FLAG_PLAYER_TEAMS;
					}
				}
			}

			if (player != NULL) {
				player->skin = NULL;
				player->shirt_color = -1;
				player->pants_color = -1;
				*last_player = player;
				last_player = &player->next;
			}
			num_players++;
		} else {
			pkt++;
		}
		complete = 1;
	}

	if ((server->num_players == 0) || (num_players > server->num_players)) {
		server->num_players = num_players;
	}

	if (!complete) {
		return (PKT_ERROR);
	} else if (server->server_name == NULL) {
		server->server_name = strdup("");
	}

	return (DONE_AUTO);
}


int
ack_descent3master_packet(struct qserver *server, char *curtok)
{
	int rc;
	char packet[0x1e];

	memcpy(packet, descent3_masterquery, 0x1a);
	packet[1] = 0x1d;
	packet[0x16] = 1;
	memcpy(packet + 0x1a, curtok, 4);
	rc = send(server->fd, packet, sizeof(packet), 0);
	if (rc == SOCKET_ERROR) {
		return (send_error(server, rc));
	}

	return (rc);
}


/* Packet from Descent3 master server (PXO)
 */
query_status_t
deal_with_descent3master_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	int i = 0, lastpacket = 0;
	char *names = rawpkt + 0x1f;
	char *ips = rawpkt + 0x29f;
	char *ports = rawpkt + 0x2ef;

	debug(2, "deal_with_descent3master_packet %p, %d", server, pktlen);

	while (i < 20) {
		if (*names) {
			char *c;
			server->master_pkt_len += 6;
			server->master_pkt = (char *)realloc(server->master_pkt, server->master_pkt_len);
			c = server->master_pkt + server->master_pkt_len - 6;
			memcpy(c, ips, 4);
			memcpy(c + 4, ports, 2);
		} else if (i > 0) {
			lastpacket = 1;
		}
		names += 0x20;
		ips += 4;
		ports += 2;
		i++;
	}

	ack_descent3master_packet(server, rawpkt + 0x1a);

	server->n_servers = server->master_pkt_len / 6;

	server->next_player_info = -1;
	server->retry1 = 0;

	if (lastpacket) {
		return (DONE_AUTO);
	}

	return (INPROGRESS);
}


/* Packet from QuakeWorld master server
 */
query_status_t
deal_with_qwmaster_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	int ret = 0;

	debug(2, "deal_with_qwmaster_packet %p, %d", server, pktlen);

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	if (rawpkt[0] == QW_NACK) {
		server->error = strdup(&rawpkt[2]);
		server->server_name = SERVERERROR;
		return (PKT_ERROR);
	}

	if (*((unsigned int *)rawpkt) == 0xffffffff) {
		rawpkt += 4;/* QW 1.5 */
		pktlen -= 4;
		if ((rawpkt[0] == '\377') && (rawpkt[1] == QW_SERVERS)) {
			rawpkt++;       /* hwmaster */
			pktlen--;
		}
	}

	if ((rawpkt[0] == QW_SERVERS) && (rawpkt[1] == QW_NEWLINE)) {
		rawpkt += 2;
		pktlen -= 2;
	} else if ((rawpkt[0] == HL_SERVERS) && (rawpkt[1] == 0x0d)) {
		// 2 byte id + 4 byte sequence
		memcpy(server->master_query_tag, rawpkt + 2, 3);
		rawpkt += 6;
		pktlen -= 6;
	} else if ((rawpkt[0] == HL_SERVERS) && (rawpkt[1] == 0x0a)) {
		// no sequence id for steam
		// instead we use the ip:port of the last recieved server
		struct in_addr *sin_addr = (struct in_addr *)(rawpkt + pktlen - 6);
		char *ip = inet_ntoa(*sin_addr);
		unsigned short port = htons(*((unsigned short *)(rawpkt + pktlen - 2)));

		//fprintf( stderr, "NEXT IP=%s:%u\n", ip, port );
		sprintf(server->master_query_tag, "%s:%u", ip, port);

		// skip over the 2 byte id
		rawpkt += 2;
		pktlen -= 2;
	} else if (strncmp(rawpkt, "servers", 7) == 0) {
		rawpkt += 8;
		pktlen -= 8;
	} else if (strncmp(rawpkt, "getserversResponse", 18) == 0) {
		rawpkt += 18;
		pktlen -= 18;

		for ( ; *rawpkt != '\\' && pktlen; pktlen--, rawpkt++) {
		}

		if (!pktlen) {
			return (1);
		}
		rawpkt++;
		pktlen--;

		debug(2, "q3m pktlen %d lastchar %x\n", pktlen, (unsigned int)rawpkt[pktlen - 1]);

		server->master_pkt = (char *)realloc(server->master_pkt, server->master_pkt_len + pktlen + 1);

		if (server->type->id == STEF_MASTER) {
			ret = decode_stefmaster_packet(server, rawpkt, pktlen);
		} else {
			ret = decode_q3master_packet(server, rawpkt, pktlen);
		}
		debug(2, "q3m %d servers\n", server->n_servers);

		return (ret);
	} else if (show_errors) {
		unsigned int ipaddr = ntohl(server->ipaddr);
		fprintf(stderr, "Odd packet from QW master %d.%d.%d.%d, processing ...\n",
		    (ipaddr >> 24) & 0xff,
		    (ipaddr >> 16) & 0xff,
		    (ipaddr >>  8) & 0xff,
		    ipaddr & 0xff
		    );
		print_packet(server, rawpkt, pktlen);
	}

	server->master_pkt = (char *)realloc(server->master_pkt, server->master_pkt_len + pktlen + 1);
	rawpkt[pktlen] = '\0';
	memcpy(server->master_pkt + server->master_pkt_len, rawpkt, pktlen + 1);
	server->master_pkt_len += pktlen;

	server->n_servers = server->master_pkt_len / 6;

	if (server->type->flags & TF_MASTER_MULTI_RESPONSE) {
		server->next_player_info = -1;
		server->retry1 = 0;
	} else if (server->type->id == HL_MASTER) {
		if ((server->master_query_tag[0] == 0) && (server->master_query_tag[1] == 0) && (server->master_query_tag[2] == 0)) {
			// all done
			server->server_name = MASTER;
			bind_sockets();
			ret = DONE_FORCE;
		} else {
			// more to come
			server->retry1++;
			send_qwmaster_request_packet(server);
		}
	} else if (server->type->flags & TF_MASTER_STEAM) {
		// should the HL_MASTER be the same as this?
		int i;
		for (i = pktlen - 6; i < pktlen && 0x00 == rawpkt[i]; i++) {
		}

		if (i == pktlen) {
			// last 6 bytes where 0x00 so we have reached the last packet
			server->n_servers--;
			server->master_pkt_len -= 6;
			server->server_name = MASTER;
			bind_sockets();
			ret = DONE_FORCE;
		} else {
			// more to come
			server->retry1++;
			send_qwmaster_request_packet(server);
		}
	} else {
		server->server_name = MASTER;
		ret = DONE_AUTO;
		bind_sockets();
	}

	return (ret);
}


int
decode_q3master_packet(struct qserver *server, char *pkt, int pktlen)
{
	char *p;
	char *end = pkt + pktlen;
	char *last = end - 6;

	pkt[pktlen] = 0;
	p = pkt;

	while (p < last) {
		// IP & Port
		memcpy(server->master_pkt + server->master_pkt_len, &p[0], 6);
		server->master_pkt_len += 6;
		p += 6;
		// Sometimes we get some bad IP's so we search for the entry terminator '\' to avoid issues with this
		while (p < end && *p != '\\') {
			p++;
		}

		if (p < end) {
			// Skip over the '\'
			p++;
		}

		if (*p && (p + 3 == end) && (0 == strncmp("EOF", p, 3))) {
			// Last packet ID ( seen in COD4 )
			server->n_servers = server->master_pkt_len / 6;
			server->retry1 = 0;     // received at least one packet so no need to retry
			return (DONE_FORCE);
		}
	}

	server->n_servers = server->master_pkt_len / 6;
	//  server->next_player_info= -1; evil, causes busy loop!
	server->retry1 = 0;     // received at least one packet so no need to retry

	return (INPROGRESS);
}


int
decode_stefmaster_packet(struct qserver *server, char *pkt, int pktlen)
{
	unsigned char *p, *m, *end;
	unsigned int i, b;

	pkt[pktlen] = 0;

	p = (unsigned char *)pkt;
	m = (unsigned char *)server->master_pkt + server->master_pkt_len;
	end = (unsigned char *)&pkt[pktlen - 12];
	while (*p && p < end) {
		for (i = 6; i; i--) {
			sscanf((char *)p, "%2x", &b);
			p += 2;
			*m++ = b;
		}
		server->master_pkt_len += 6;
		while (*p && *p == '\\') {
			p++;
		}
	}
	server->n_servers = server->master_pkt_len / 6;
	server->next_player_info = -1;
	server->retry1 = 0;

	return (1);
}


/* Packet from Tribes master server
 */
query_status_t
deal_with_tribesmaster_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned char *upkt = (unsigned char *)rawpkt;
	int packet_number = upkt[2];
	int n_packets = upkt[3];
	unsigned char *p;
	char *mpkt;
	int len;
	unsigned int ipaddr;

	debug(2, "deal_with_tribesmaster_packet %p, %d", server, pktlen);

	if (memcmp(rawpkt, tribes_master_response, sizeof(tribes_master_response)) != 0) {
		fprintf(stderr, "Odd packet from Tribes master server\n");
		print_packet(server, rawpkt, pktlen);
	}

	/*	0x1006
	 *  01		packet number
	 *  08		# packets
	 *  02
	 *  0000
	 *  66
	 *  0d		length of following string
	 *  "Tribes Master"
	 *  3c		length of following string
	 *  "Check out the Starsiege demo now!   www.starsiegeplayers.com"
	 *  0035
	 *  06 d143 4764 812c
	 *  06 d1e2 8df3 616d
	 *  06 1804 6d50 616d
	 *  06 d81c 6dc0 616d
	 */

	/*	0x1006
	 *  02
	 *  08
	 *  02 0000
	 *  66
	 *  00 3f
	 *  06 cf88 344c 1227
	 */

	/* printf( "packet_number %d n_packets %d\n", packet_number, n_packets);
	 */

	len = upkt[8];
	if (len > 0) {
		p = (unsigned char *)rawpkt + 9;
		// printf( "%.*s\n", len, p);
		p += len;
		len = upkt[8 + len + 1];
		// printf( "%.*s\n", len, p+1);
		p += len + 1;
		p += 2;
	} else {
		p = (unsigned char *)rawpkt + 10;
	}

	if (server->master_pkt == NULL) {
		server->master_pkt = (char *)malloc(n_packets * 64 * 6);
		mpkt = server->master_pkt;
	} else {
		mpkt = server->master_pkt + server->n_servers * 6;
	}

	while ((char *)p < rawpkt + pktlen) {
		if (*p != 0x6) {
			printf("*p %u\n", (unsigned)*p);
		}
		memcpy(mpkt, p + 1, sizeof(ipaddr));
		if (0) {
			mpkt[4] = p[5];
			mpkt[5] = p[6];
		} else {
			mpkt[5] = p[5];
			mpkt[4] = p[6];
		}
		//printf( "%08x:%hu %u.%u.%u.%u:%hu\n", ipaddr, port, ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff, port);
		p += 7;
		mpkt += 6;
	}

	/*
	 * if ( (char*)p != rawpkt+pktlen)
	 * printf( "%x %x\n", p, rawpkt+pktlen);
	 */
	server->master_pkt_len = mpkt - server->master_pkt;
	server->n_servers = server->master_pkt_len / 6;
	server->server_name = MASTER;
	server->next_player_info = -1;

	if (packet_number >= n_packets) {
		return (DONE_FORCE);
	} else {
		return (DONE_AUTO);
	}
}


char *
display_tribes2_string_list(unsigned char *pkt)
{
	char *delim = "";
	unsigned int count, len;

	count = *pkt;
	pkt++;
	for ( ; count; count--) {
		len = *pkt;
		pkt++;
		if (len > 0) {
			if (raw_display) {
				xform_printf(OF, "%s%.*s", delim, (int)len, pkt);
				delim = raw_delimiter;
			} else {
				xform_printf(OF, "%.*s\n", (int)len, pkt);
			}
		}
		pkt += len;
	}
	if (raw_display) {
		fputs("\n", OF);
	}
	return ((char *)pkt);
}


query_status_t
deal_with_tribes2master_packet(struct qserver *server, char *pkt, int pktlen)
{
	unsigned int n_servers, index, total, server_limit;
	char *p, *mpkt;

	debug(2, "deal_with_tribes2master_packet %p, %d", server, pktlen);

	if (pkt[0] == TRIBES2_RESPONSE_GAME_TYPES) {
		pkt += 6;
		if (raw_display) {
			xform_printf(OF, "%s%s%s%s", server->type->type_prefix, raw_delimiter, server->arg, raw_delimiter);
		} else {
			xform_printf(OF, "Game Types\n");
			xform_printf(OF, "----------\n");
		}
		pkt = display_tribes2_string_list((unsigned char *)pkt);
		if (raw_display) {
			xform_printf(OF, "%s%s%s%s", server->type->type_prefix, raw_delimiter, server->arg, raw_delimiter);
		} else {
			xform_printf(OF, "\nMission Types\n");
			xform_printf(OF, "-------------\n");
		}
		display_tribes2_string_list((unsigned char *)pkt);

		server->master_pkt_len = 0;
		server->n_servers = 0;
		server->server_name = MASTER;
		server->next_player_info = -1;
		return (DONE_FORCE);
	}

	if (pkt[0] != TRIBES2_RESPONSE_MASTER) {
		/* error */
		return (PKT_ERROR);
	}

	server_limit = get_param_ui_value(server, "limit", ~0);

	n_servers = little_endian ? *(unsigned short *)(pkt + 8) : swap_short(pkt + 8);
	index = *(unsigned char *)(pkt + 6);
	total = *(unsigned char *)(pkt + 7);
	if (server->master_pkt == NULL) {
		server->master_pkt = (char *)malloc(total * n_servers * 6);
		mpkt = server->master_pkt;
	} else {
		mpkt = server->master_pkt + server->n_servers * 6;
	}

	p = pkt + 10;
	for ( ; n_servers && ((char *)mpkt - server->master_pkt) / 6 < server_limit; n_servers--, p += 6, mpkt += 6) {
		memcpy(mpkt, p, 4);
		mpkt[4] = p[5];
		mpkt[5] = p[4];
	}
	server->master_pkt_len = (char *)mpkt - server->master_pkt;
	server->n_servers = server->master_pkt_len / 6;
	server->server_name = MASTER;
	server->next_player_info = -1;

	if ((index >= total - 1) || (server->n_servers >= server_limit)) {
		return (PKT_ERROR);
	}

	return (DONE_AUTO);
}


int
server_info_packet(struct qserver *server, struct q_packet *pkt, int datalen)
{
	int off = 0;

	/* ignore duplicate packets */
	if (server->server_name != NULL) {
		return (0);
	}

	server->address = strdup((char *)&pkt->data[off]);
	off += strlen(server->address) + 1;
	if (off >= datalen) {
		return (-1);
	}

	server->server_name = strdup((char *)&pkt->data[off]);
	off += strlen(server->server_name) + 1;
	if (off >= datalen) {
		return (-1);
	}

	server->map_name = strdup((char *)&pkt->data[off]);
	off += strlen(server->map_name) + 1;
	if (off > datalen) {
		return (-1);
	}

	server->num_players = pkt->data[off++];
	server->max_players = pkt->data[off++];
	server->protocol_version = pkt->data[off++];

	server->retry1 = n_retries;

	if (get_server_rules) {
		debug(3, "send_rule_request_packet3");
		send_rule_request_packet(server);
	}
	if (get_player_info) {
		send_player_request_packet(server);
	}

	return (0);
}


int
player_info_packet(struct qserver *server, struct q_packet *pkt, int datalen)
{
	char *name, *address;
	int off, colors, frags, connect_time, player_number;
	struct player *player, *last;

	off = 0;
	player_number = pkt->data[off++];
	name = (char *)&pkt->data[off];
	off += strlen(name) + 1;
	if (off >= datalen) {
		return (-1);
	}

	colors = pkt->data[off + 3];
	colors = (colors << 8) + pkt->data[off + 2];
	colors = (colors << 8) + pkt->data[off + 1];
	colors = (colors << 8) + pkt->data[off];
	off += sizeof(colors);

	frags = pkt->data[off + 3];
	frags = (frags << 8) + pkt->data[off + 2];
	frags = (frags << 8) + pkt->data[off + 1];
	frags = (frags << 8) + pkt->data[off];
	off += sizeof(frags);

	connect_time = pkt->data[off + 3];
	connect_time = (connect_time << 8) + pkt->data[off + 2];
	connect_time = (connect_time << 8) + pkt->data[off + 1];
	connect_time = (connect_time << 8) + pkt->data[off];
	off += sizeof(connect_time);

	address = (char *)&pkt->data[off];
	off += strlen(address) + 1;
	if (off > datalen) {
		return (-1);
	}

	last = server->players;
	while (last != NULL && last->next != NULL) {
		if (last->number == player_number) {
			return (0);
		}
		last = last->next;
	}

	if ((last != NULL) && (last->number == player_number)) {
		return (0);
	}

	player = (struct player *)calloc(1, sizeof(struct player));
	player->number = player_number;
	player->name = strdup(name);
	player->address = strdup(address);
	player->connect_time = connect_time;
	player->frags = frags;
	player->shirt_color = colors >> 4;
	player->pants_color = colors & 0xf;
	player->next = NULL;

	if (last == NULL) {
		server->players = player;
	} else {
		last->next = player;
	}

	server->next_player_info++;
	server->retry2 = n_retries;
	if (server->next_player_info < server->num_players) {
		send_player_request_packet(server);
	}

	return (0);
}


int
rule_info_packet(struct qserver *server, struct q_packet *pkt, int datalen)
{
	int off = 0;
	struct rule *rule, *last;
	char *name, *value;

	/* Straggler packet after we've already given up fetching rules */
	if (server->next_rule == NULL) {
		return (0);
	}

	if (ntohs(pkt->length) == Q_HEADER_LEN) {
		server->next_rule = NULL;
		return (0);
	}

	name = (char *)&pkt->data[off];
	off += strlen(name) + 1;
	if (off >= datalen) {
		return (-1);
	}

	value = (char *)&pkt->data[off];
	off += strlen(value) + 1;
	if (off > datalen) {
		return (-1);
	}

	last = server->rules;
	while (last != NULL && last->next != NULL) {
		if (strcmp(last->name, name) == 0) {
			return (0);
		}
		last = last->next;
	}
	if ((last != NULL) && (strcmp(last->name, name) == 0)) {
		return (0);
	}

	rule = (struct rule *)malloc(sizeof(struct rule));
	rule->name = strdup(name);
	rule->value = strdup(value);
	rule->next = NULL;

	if (last == NULL) {
		server->rules = rule;
	} else {
		last->next = rule;
	}

	server->n_rules++;
	server->next_rule = rule->name;
	server->retry1 = n_retries;
	debug(3, "send_rule_request_packet4");
	send_rule_request_packet(server);

	return (0);
}


struct info *
player_add_info(struct player *player, char *key, char *value, int flags)
{
	struct info *info;

	if (flags & OVERWITE_DUPLICATES) {
		for (info = player->info; info; info = info->next) {
			if (0 == strcmp(info->name, key)) {
				// We should be able to free this
				free(info->value);
				if (flags & NO_VALUE_COPY) {
					info->value = value;
				} else {
					info->value = strdup(value);
				}

				return (info);
			}
		}
	}

	if (flags & CHECK_DUPLICATE_RULES) {
		for (info = player->info; info; info = info->next) {
			if (0 == strcmp(info->name, key)) {
				return (NULL);
			}
		}
	}

	if (flags & COMBINE_VALUES) {
		for (info = player->info; info; info = info->next) {
			if (0 == strcmp(info->name, key)) {
				char *full_value = (char *)calloc(sizeof(char), strlen(info->value) + strlen(value) + 2);
				if (NULL == full_value) {
					fprintf(stderr, "Failed to malloc combined value\n");
					exit(1);
				}
				sprintf(full_value, "%s%s%s", info->value, multi_delimiter, value);

				// We should be able to free this
				free(info->value);
				info->value = full_value;

				return (info);
			}
		}
	}

	info = (struct info *)malloc(sizeof(struct info));
	if (flags & NO_KEY_COPY) {
		info->name = key;
	} else {
		info->name = strdup(key);
	}
	if (flags & NO_VALUE_COPY) {
		info->value = value;
	} else {
		info->value = strdup(value);
	}
	info->next = NULL;

	if (NULL == player->info) {
		player->info = info;
	} else {
		*player->last_info = info;
	}
	player->last_info = &info->next;
	player->n_info++;

	return (info);
}


struct rule *
add_rule(struct qserver *server, char *key, char *value, int flags)
{
	struct rule *rule;

	debug(3, "key: %s, value: %s, flags: %d", key, value, flags);
	if (flags & OVERWITE_DUPLICATES) {
		for (rule = server->rules; rule; rule = rule->next) {
			if (0 == strcmp(rule->name, key)) {
				// We should be able to free this
				free(rule->value);
				if (flags & NO_VALUE_COPY) {
					rule->value = value;
				} else {
					rule->value = strdup(value);
				}

				return (rule);
			}
		}
	}

	if (flags & CHECK_DUPLICATE_RULES) {
		for (rule = server->rules; rule; rule = rule->next) {
			if (0 == strcmp(rule->name, key)) {
				return (NULL);
			}
		}
	}

	if (flags & COMBINE_VALUES) {
		for (rule = server->rules; rule; rule = rule->next) {
			if (0 == strcmp(rule->name, key)) {
				char *full_value = (char *)calloc(sizeof(char), strlen(rule->value) + strlen(value) + strlen(multi_delimiter) + 1);
				if (NULL == full_value) {
					fprintf(stderr, "Failed to malloc combined value\n");
					exit(1);
				}
				sprintf(full_value, "%s%s%s", rule->value, multi_delimiter, value);

				// We should be able to free this
				free(rule->value);
				rule->value = full_value;

				return (rule);
			}
		}
	}

	rule = (struct rule *)malloc(sizeof(struct rule));
	if (flags & NO_KEY_COPY) {
		rule->name = key;
	} else {
		rule->name = strdup(key);
	}

	if (flags & NO_VALUE_COPY) {
		rule->value = value;
	} else {
		rule->value = strdup(value);
	}
	rule->next = NULL;
	*server->last_rule = rule;
	server->last_rule = &rule->next;
	server->n_rules++;

	return (rule);
}


void
add_nrule(struct qserver *server, char *key, char *value, int len)
{
	struct rule *rule;

	for (rule = server->rules; rule; rule = rule->next) {
		if (strcmp(rule->name, key) == 0) {
			return;
		}
	}

	rule = (struct rule *)malloc(sizeof(struct rule));
	rule->name = strdup(key);
	rule->value = strndup(value, len);
	rule->next = NULL;
	*server->last_rule = rule;
	server->last_rule = &rule->next;
	server->n_rules++;
}


struct player *
add_player(struct qserver *server, int player_number)
{
	struct player *player;

	for (player = server->players; player; player = player->next) {
		if (player->number == player_number) {
			return (NULL);
		}
	}

	player = (struct player *)calloc(1, sizeof(struct player));
	player->number = player_number;
	player->next = server->players;
	player->n_info = 0;
	player->ping = NA_INT;
	player->team = NA_INT;
	player->score = NA_INT;
	player->deaths = NA_INT;
	player->frags = NA_INT;
	player->last_info = NULL;
	server->players = player;
	server->n_player_info++;
	return (player);
}


STATIC struct player *
get_player_by_number(struct qserver *server, int player_number)
{
	struct player *player;

	for (player = server->players; player; player = player->next) {
		if (player->number == player_number) {
			return (player);
		}
	}
	return (NULL);
}


// Updates a servers port information.
// Sets the rules:
// _queryport <queryport>
// hostport <port>
void
change_server_port(struct qserver *server, unsigned short port, int force)
{
	if ((port > 0) && (port != server->port)) {
		// valid port and changing
		char arg[64];
		unsigned int ipaddr = ntohl(server->ipaddr);

		// Update the servers hostname as required
		sprintf(arg, "%d.%d.%d.%d:%hu", ipaddr >> 24, (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff, port);

		if (show_game_port || force || server->flags & TF_SHOW_GAME_PORT) {
			// Update the server arg
			free(server->arg);
			server->arg = strdup(arg);

			// Add a rule noting the previous query port
			sprintf(arg, "%hu", server->port);
			add_rule(server, "_queryport", arg, NO_FLAGS);

			// Update the servers port
			server->port = port;
		}

		if (0 != strcmp(server->arg, server->host_name)) {
			// hostname isnt the query arg
			char *colon = strchr(server->host_name, ':');
			// dns hostname or hostname:port
			char *hostname = malloc(strlen(server->host_name) + 7);
			if (NULL == hostname) {
				fprintf(stderr, "Failed to malloc hostname memory\n");
			} else {
				if (colon) {
					*colon = '\0';
				}
				sprintf(hostname, "%s:%hu", server->host_name, port);
				free(server->host_name);
				server->host_name = hostname;
			}
		}

		// Add a rule noting the servers hostport
		sprintf(arg, "%hu", port);
		add_rule(server, "hostport", arg, OVERWITE_DUPLICATES);
	}
}


STATIC void
players_set_teamname(struct qserver *server, int teamid, char *teamname)
{
	struct player *player;

	for (player = server->players; player; player = player->next) {
		if (player->team == teamid) {
			player->team_name = strdup(teamname);
		}
	}
}


STATIC char *
dup_nstring(const char *pkt, const char *end, char **next)
{
	char *pt = (char *)pkt;
	int len = ((unsigned char *)pkt)[0];

	pt++;
	if (*pt == '\1') {
		len++;
	}
	if (pt + len > end) {
		return (NULL);
	}

	*next = pt + len;
	return (strndup(pt, len));
}


STATIC char *
dup_n1string(char *pkt, char *end, char **next)
{
	unsigned len;

	if (!pkt || (pkt >= end)) {
		return (NULL);
	}

	len = (unsigned char)pkt[0] - 1;
	pkt++;
	if (pkt + len > end) {
		return (NULL);
	}

	*next = pkt + len;
	return (strndup(pkt, len));
}


STATIC int
pariah_basic_packet(struct qserver *server, char *rawpkt, char *end)
{
	char *next;
	char *string;

	change_server_port(server, swap_short_from_little(&rawpkt[14]), 0);
	if (NULL == (string = ut2003_strdup(&rawpkt[18], end, &next))) {
		return (-1);
	}

	if (server->server_name == NULL) {
		server->server_name = string;
	} else {
		free(string);
	}

	if (NULL == (string = ut2003_strdup(next, end, &next))) {
		return (-1);
	}

	if (server->map_name == NULL) {
		server->map_name = string;
	} else {
		free(string);
	}

	if (NULL == (string = ut2003_strdup(next, end, &next))) {
		return (-1);
	}

	if (server->game == NULL) {
		server->game = string;
		add_rule(server, "gametype", server->game, NO_FLAGS | CHECK_DUPLICATE_RULES);
	} else {
		free(string);
	}

	server->num_players = (unsigned char)next[0];
	server->max_players = (unsigned char)next[1];

	return (0);
}


STATIC int
ut2003_basic_packet(struct qserver *server, char *rawpkt, char *end)
{
	char *next;
	char *string;

	change_server_port(server, swap_short_from_little(&rawpkt[6]), 0);

	if (NULL == (string = ut2003_strdup(&rawpkt[14], end, &next))) {
		return (-1);
	}

	if (server->server_name == NULL) {
		server->server_name = string;
	} else {
		free(string);
	}

	if (NULL == (string = ut2003_strdup(next, end, &next))) {
		return (-1);
	}

	if (server->map_name == NULL) {
		server->map_name = string;
	} else {
		free(string);
	}

	if (NULL == (string = ut2003_strdup(next, end, &next))) {
		return (-1);
	}

	if (server->game == NULL) {
		server->game = string;
		add_rule(server, "gametype", server->game, NO_FLAGS | CHECK_DUPLICATE_RULES);
	} else {
		free(string);
	}

	server->num_players = swap_long_from_little(next);
	next += 4;
	server->max_players = swap_long_from_little(next);
	return (0);
}


STATIC int
pariah_rule_packet(struct qserver *server, char *rawpkt, char *end)
{
	char *key, *value;

	unsigned char no_rules = (unsigned char)rawpkt[1];
	unsigned char seen = 0;

	// type + no_rules
	rawpkt += 2;

	// we get size encoded key = value pairs
	while (rawpkt < end && no_rules > seen) {
		// first byte is the rule count
		seen = (unsigned char)rawpkt[0];
		rawpkt++;
		if (NULL == (key = ut2003_strdup(rawpkt, end, &rawpkt))) {
			break;
		}

		if ('\0' == rawpkt[0]) {
			value = strdup("");
			rawpkt++;
		} else if (NULL == (value = ut2003_strdup(rawpkt, end, &rawpkt))) {
			break;
		}

		if (NULL == add_rule(server, key, value, NO_KEY_COPY | NO_VALUE_COPY | COMBINE_VALUES)) {
			/* duplicate, so free key and value */
			free(value);
			free(key);
		}

		seen++;
	}

	if (no_rules == seen) {
		// all done
		server->next_rule = NULL;
		return (1);
	}

	return (0);
}


STATIC int
ut2003_rule_packet(struct qserver *server, char *rawpkt, char *end)
{
	char *key, *value;
	int result = 0;

	// Packet Type
	rawpkt++;

	// we get size encoded key = value pairs
	while (rawpkt < end) {
		if (NULL == (key = ut2003_strdup(rawpkt, end, &rawpkt))) {
			break;
		}

		if (NULL == (value = ut2003_strdup(rawpkt, end, &rawpkt))) {
			break;
		}

		if (strcmp(key, "minplayers") == 0) {
			result = atoi(value);
		}

		if (NULL == add_rule(server, key, value, NO_KEY_COPY | NO_VALUE_COPY | COMBINE_VALUES)) {
			/* duplicate, so free key and value */
			free(value);
			free(key);
		}
	}

	return (result);
}


char *
ut2003_strdup(const char *string, const char *end, char **next)
{
	unsigned char len = string[0];
	char *result = NULL;

	if (len < 128) {
		// type 1 string
		//fprintf( stderr, "Type 1:" );
		result = dup_nstring(string, end, next);
	} else {
		// type 2 string
		//fprintf( stderr, "Type 2:\n" );
		const char *last;
		char *resp, *pos;
		// minus indicator
		len -= 128;
		// double byte chars so * 2
		len = len * 2;
		last = string + len;
		if (last > end) {
			*next = (char *)end;
			fprintf(stderr, "Type 2 string format error ( too short )\n");
			return (NULL);
		}

		*next = (char *)last + 1;
		if (NULL == (result = (char *)calloc(last - string, sizeof(char)))) {
			fprintf(stderr, "Failed to malloc string memory\n");
			return (NULL);
		}
		resp = result;
		pos = (char *)string + 1;
		while (pos <= last) {
			// check for a color code
			if ((pos + 6 <= last) && (0 == memcmp(pos, "^\0#\0", 4))) {
				// we have a color code
				//fprintf( stderr, "color:%02hhx%02hhx\n", pos[4], pos[5] );
				// indicator transformed to ^\1
				*resp = *pos;
				resp++;
				pos++;
				*resp = '\1';
				resp++;
				pos += 3;
				// color byte
				*resp = *pos;
				resp++;
				pos += 2;
				//pos += 6;
			}

			// standard char
			//fprintf( stderr, "char: %02hhx\n", *pos );
			*resp = *pos;
			resp++;
			pos += 2;
		}
	}

	//fprintf( stderr, "'%s'\n", result );

	return (result);
}


STATIC int
pariah_player_packet(struct qserver *server, char *rawpkt, char *end)
{
	unsigned char no_players = rawpkt[1];
	unsigned char seen = 0; /* XXX: cannot work this way, it takes only
	                         *                         this packet into consideration. What if
	                         *                         player info is spread across multiple
	                         *                         packets? */

	// type + no_players + some unknown preamble
	rawpkt += 3;
	while (rawpkt < end && seen < no_players) {
		struct player *player;

		// Player Number
		rawpkt += 4;

		// Create a player
		if (NULL == (player = add_player(server, server->n_player_info))) {
			return (0);
		}

		// Name ( min 3 bytes )
		player->name = ut2003_strdup(rawpkt, end, &rawpkt);

		// Ping
		player->ping = swap_long_from_little(rawpkt);
		rawpkt += 4;

		// Frags
		player->frags = (unsigned char)rawpkt[0];
		rawpkt++;

		// unknown
		rawpkt++;

		seen++;
	}

	if (no_players == seen) {
		// all done
		server->num_players = server->n_player_info;
		return (1);
	}

	// possibly more to come

	return (0);
}


STATIC int
ut2003_player_packet(struct qserver *server, char *rawpkt, char *end)
{
	// skip type
	rawpkt++;
	switch (server->protocol_version) {
	case 0x7e:
		// XMP packet
		//fprintf( stderr, "XMP packet\n" );
		while (rawpkt < end) {
			struct player *player;
			char *var, *val;
			unsigned char no_props;
			if (rawpkt + 24 > end) {
				malformed_packet(server, "player info too short");
				rawpkt = end;
				return (1);
			}

			// Player Number never set
			rawpkt += 4;

			// Player ID never set
			rawpkt += 4;

			if (NULL == (player = add_player(server, server->n_player_info))) {
				return (0);
			}

			// Name ( min 3 bytes )
			player->name = ut2003_strdup(rawpkt, end, &rawpkt);

			// Ping
			player->ping = swap_long_from_little(rawpkt);
			rawpkt += 4;

			// Frags
			player->frags = swap_long_from_little(rawpkt);
			rawpkt += 4;

			// Stat ID never set
			rawpkt += 4;

			// Player properties
			no_props = rawpkt[0];
			//fprintf( stderr, "noprops %d\n", no_props );
			rawpkt++;
			while (rawpkt < end && no_props > 0) {
				if (NULL == (var = ut2003_strdup(rawpkt, end, &rawpkt))) {
					break;
				}
				if (NULL == (val = ut2003_strdup(rawpkt, end, &rawpkt))) {
					break;
				}
				//fprintf( stderr, "attrib: %s = %s\n", var, val );

				// Things we can use
				if (0 == strcmp(var, "team")) {
					player->team_name = val;
				} else if (0 == strcmp(var, "class")) {
					player->skin = val;
				} else {
					free(val);
				}

				free(var);
				no_props--;
			}
		}
		break;

	default:
		while (rawpkt < end) {
			struct player *player;

			if (rawpkt + 4 > end) {
				malformed_packet(server, "player packet too short");
				return (1);
			}

			if (NULL == (player = add_player(server, swap_long_from_little(rawpkt)))) {
				return (0);
			}

			player->name = ut2003_strdup(rawpkt + 4, end, &rawpkt);
			if (rawpkt + 8 > end) {
				malformed_packet(server, "player packet too short");
				return (1);
			}
			player->ping = swap_long_from_little(rawpkt);
			rawpkt += 4;
			player->frags = swap_long_from_little(rawpkt);
			rawpkt += 4;
			{
				unsigned team = swap_long_from_little(rawpkt);
				rawpkt += 4;
				player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
				if (team & 1 << 29) {
					player->team_name = "red";
				} else if (team & 1 << 30) {
					player->team_name = "blue";
				}
			}
		}
	}

	return (0);
}


char *
get_rule(struct qserver *server, char *name)
{
	struct rule *rule;

	rule = server->rules;
	for ( ; rule != NULL; rule = rule->next) {
		if (strcmp(name, rule->name) == 0) {
			return (rule->value);
		}
	}

	return (NULL);
}


query_status_t
deal_with_ut2003_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	// For protocol spec see:
	// http://unreal.student.utwente.nl/UT2003-queryspec.html

	char *end;
	int error = 0, before;
	unsigned int packet_header;

	debug(2, "deal_with_ut2003_packet %p, %d", server, pktlen);

	rawpkt[pktlen] = '\0';
	end = &rawpkt[pktlen];

	packet_header = swap_long_from_little(&rawpkt[0]);
	rawpkt += 4;

	server->protocol_version = packet_header;
	if (
		(packet_header != 0x77) &&      // Pariah Demo?
		(packet_header != 0x78) &&      // UT2003 Demo
		(packet_header != 0x79) &&      // UT2003 Retail
		(packet_header != 0x7e) &&      // Unreal2 XMP
		(packet_header != 0x7f) &&      // UT2004 Demo
		(packet_header != 0x80)         // UT2004 Retail
		) {
		malformed_packet(server, "Unknown type 0x%x", packet_header);
	}

	switch (rawpkt[0]) {
	case 0x00:
		// Server info
		if (server->server_name == NULL) {
			server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		}

		error = ut2003_basic_packet(server, rawpkt, end);
		if (!error) {
			if (get_server_rules || get_player_info) {
				int requests = server->n_requests;
				server->next_rule = "";
				server->retry1 = n_retries;
				server->retry2 = 0;             // don't wait for player packet
				debug(3, "send_rule_request_packet5");
				send_rule_request_packet(server);
				server->n_requests = requests;          // would produce wrong ping
			}
		}
		break;

	case 0x01:
		// Game info
		ut2003_rule_packet(server, rawpkt, end);
		server->next_rule = "";
		server->retry1 = 0;             /* we received at least one rule packet so
		                                 *                 no need to retry. We'd get double
		                                 *                 entries otherwise. */
		break;

	case 0x02:
		// Player info
		before = server->n_player_info;
		error = ut2003_player_packet(server, rawpkt, end);
		if (before == server->n_player_info) {
			error = 1;
		}
		break;

	case 0x10:
		// Pariah Server info
		if (server->server_name == NULL) {
			server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
		}

		error = pariah_basic_packet(server, rawpkt, end);
		if (!error) {
			// N.B. pariah always sends a rules and players packet
			int requests = server->n_requests;
			server->next_rule = "";
			server->retry1 = n_retries;
			server->retry2 = 0;
			server->n_requests = requests;          // would produce wrong ping
		}
		break;

	case 0x11:
		// Game info
		pariah_rule_packet(server, rawpkt, end);
		server->retry1 = 0;             /* we received at least one rule packet so
		                                 *                 no need to retry. We'd get double
		                                 *                 entries otherwise. */
		break;

	case 0x12:
		// Player info
		before = server->n_player_info;
		pariah_player_packet(server, rawpkt, end);
		if (before == server->n_player_info) {
			error = 1;
		}
		break;

	default:
		malformed_packet(server, "Unknown packet type 0x%x", (unsigned)rawpkt[0]);
		break;
	}

	/* don't cleanup if we fetch server rules. We would lose
	 * rule packets as we don't know how many we get
	 * We do clean up if we don't fetch server rules so we don't
	 * need to wait for timeout.
	 */
	if (
		error ||
		(!get_server_rules && !get_player_info) ||
		(!get_server_rules && (server->num_players == server->n_player_info)) ||
		((server->next_rule == NULL) && (server->num_players == server->n_player_info))
		) {
		return (DONE_FORCE);
	}

	return (INPROGRESS);
}


int
deal_with_unrealmaster_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	debug(2, "deal_with_unrealmaster_packet %p, %d", server, pktlen);

	if (pktlen == 0) {
		return (PKT_ERROR);
	}
	print_packet(server, rawpkt, pktlen);
	puts("--");
	return (0);
}


/* Returns 1 if the query is done (server may be freed) and 0 if not.
 */
query_status_t
deal_with_halflife_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *pkt;
	char *end = &rawpkt[pktlen];
	int pkt_index = 0, pkt_max = 0;
	char number[16];
	short pkt_id;

	debug(2, "deal_with_halflife_packet %p, %d", server, pktlen);

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	}

	if (pktlen < 5) {
		return (PKT_ERROR);
	}

	if ((((rawpkt[0] != '\377') && (rawpkt[0] != '\376')) || (rawpkt[1] != '\377') || (rawpkt[2] != '\377') || (rawpkt[3] != '\377')) && show_errors) {
		unsigned int ipaddr = ntohl(server->ipaddr);
		fprintf(stderr, "Odd packet from server %d.%d.%d.%d:%hu, processing ...\n",
		    (ipaddr >> 24) & 0xff,
		    (ipaddr >> 16) & 0xff,
		    (ipaddr >> 8) & 0xff,
		    ipaddr & 0xff,
		    ntohs(server->port)
		    );
		print_packet(server, rawpkt, pktlen);
	}

	if (((unsigned char *)rawpkt)[0] == 0xfe) {
		SavedData *sdata;
		pkt_index = ((unsigned char *)rawpkt)[8] >> 4;
		pkt_max = ((unsigned char *)rawpkt)[8] & 0xf;
		memcpy(&pkt_id, &rawpkt[4], 2);

		if (server->saved_data.data == NULL) {
			sdata = &server->saved_data;
		} else {
			sdata = (SavedData *)calloc(1, sizeof(SavedData));
			sdata->next = server->saved_data.next;
			server->saved_data.next = sdata;
		}

		sdata->pkt_index = pkt_index;
		sdata->pkt_max = pkt_max;
		sdata->pkt_id = pkt_id;
		sdata->datalen = pktlen - 9;
		sdata->data = (char *)malloc(pktlen - 9);
		memcpy(sdata->data, &rawpkt[9], pktlen - 9);

		/* combine_packets will call us recursively */
		return (combine_packets(server));

		/*
		 * fprintf( OF, "pkt_index %d pkt_max %d\n", pkt_index, pkt_max);
		 * rawpkt+= 9;
		 * pktlen-= 9;
		 */
	}

	/* 'info' response */
	if ((rawpkt[4] == 'C') || (rawpkt[4] == 'm')) {
		if (server->server_name != NULL) {
			return (0);
		}
		pkt = &rawpkt[5];
		server->address = strdup(pkt);
		pkt += strlen(pkt) + 1;
		server->server_name = strdup(pkt);
		pkt += strlen(pkt) + 1;
		server->map_name = strdup(pkt);
		pkt += strlen(pkt) + 1;

		if (*pkt) {
			add_rule(server, "gamedir", pkt, NO_FLAGS);
		}
		if (*pkt && (strcmp(pkt, "valve") != 0)) {
			server->game = add_rule(server, "game", pkt, NO_FLAGS)->value;
			server->flags |= FLAG_DO_NOT_FREE_GAME;
		}
		pkt += strlen(pkt) + 1;
		if (*pkt) {
			add_rule(server, "gamename", pkt, NO_FLAGS);
		}
		pkt += strlen(pkt) + 1;

		server->num_players = (unsigned int)pkt[0];
		server->max_players = (unsigned int)pkt[1];
		pkt += 2;
		if (pkt < end) {
			int protocol = *((unsigned char *)pkt);
			sprintf(number, "%d", protocol);
			add_rule(server, "protocol", number, NO_FLAGS);
			pkt++;
		}

		if (rawpkt[4] == 'm') {
			if (*pkt == 'd') {
				add_rule(server, "sv_type", "dedicated", NO_FLAGS);
			} else if (*pkt == 'l') {
				add_rule(server, "sv_type", "listen", NO_FLAGS);
			} else {
				add_rule(server, "sv_type", "?", NO_FLAGS);
			}
			pkt++;
			if (*pkt == 'w') {
				add_rule(server, "sv_os", "windows", NO_FLAGS);
			} else if (*pkt == 'l') {
				add_rule(server, "sv_os", "linux", NO_FLAGS);
			} else {
				char str[2] = "\0";
				str[0] = *pkt;
				add_rule(server, "sv_os", str, NO_FLAGS);
			}
			pkt++;
			add_rule(server, "sv_password", *pkt ? "1" : "0", NO_FLAGS);
			pkt++;
			add_rule(server, "mod", *pkt ? "1" : "0", NO_FLAGS);
			if (*pkt) {
				int n;
				/* pull out the mod infomation */
				pkt++;
				add_rule(server, "mod_info_url", pkt, NO_FLAGS);
				pkt += strlen(pkt) + 1;
				if (*pkt) {
					add_rule(server, "mod_download_url", pkt, NO_FLAGS);
				}
				pkt += strlen(pkt) + 1;
				if (*pkt) {
					add_rule(server, "mod_detail", pkt, NO_FLAGS);
				}
				pkt += strlen(pkt) + 1;
				n = swap_long_from_little(pkt);
				sprintf(number, "%d", n);
				add_rule(server, "modversion", number, NO_FLAGS);
				pkt += 4;
				n = swap_long_from_little(pkt);
				sprintf(number, "%d", n);
				add_rule(server, "modsize", number, NO_FLAGS);
				pkt += 4;
				add_rule(server, "svonly", *pkt ? "1" : "0", NO_FLAGS);
				pkt++;
				add_rule(server, "cldll", *pkt ? "1" : "0", NO_FLAGS);
				pkt++;
				if (pkt < end) {
					add_rule(server, "secure", *pkt ? "1" : "0", NO_FLAGS);
				}
			}
		}

		if (get_player_info && server->num_players) {
			int requests = server->n_requests;
			server->next_player_info = server->num_players - 1;
			send_player_request_packet(server);
			server->n_requests = requests;  // prevent wrong ping
		}
		if (get_server_rules) {
			int requests = server->n_requests;
			server->next_rule = "";
			server->retry1 = n_retries;
			debug(3, "send_rule_request_packet6");
			send_rule_request_packet(server);
			server->n_requests = requests;  // prevent wrong ping
		}
	}
	/* 'players' response */
	else if ((rawpkt[4] == 'D') && (server->players == NULL)) {
		unsigned int n = 0, temp;
		struct player *player;
		struct player **last_player = &server->players;
		if ((unsigned int)rawpkt[5] > server->num_players) {
			server->num_players = (unsigned int)rawpkt[5];
		}
		pkt = &rawpkt[6];
		rawpkt[pktlen] = '\0';
		while (1) {
			if (*pkt != n + 1) {
				break;
			}
			n++;
			pkt++;
			player = (struct player *)calloc(1, sizeof(struct player));
			player->name = strdup(pkt);
			pkt += strlen(pkt) + 1;
			memcpy(&player->frags, pkt, 4);
			pkt += 4;
			memcpy(&temp, pkt, 4);
			pkt += 4;
			if (big_endian) {
				player->frags = swap_long(&player->frags);
			}
			player->connect_time = swap_float_from_little(&temp);
			*last_player = player;
			last_player = &player->next;
		}
		if (n > server->num_players) {
			server->num_players = n;
		}
		server->next_player_info = server->num_players;
	}
	/* 'rules' response */
	else if ((rawpkt[4] == 'E') && (server->next_rule != NULL)) {
		int n = 0;
		n = ((unsigned char *)rawpkt)[5] + ((unsigned char *)rawpkt)[6] * 256;
		pkt = &rawpkt[7];
		while (n) {
			char *key = pkt;
			char *value;
			pkt += strlen(pkt) + 1;
			if (pkt > end) {
				break;
			}
			value = pkt;
			pkt += strlen(pkt) + 1;
			if (pkt > end) {
				break;
			}
			if ((key[0] == 's') && (strcmp(key, "sv_password") == 0)) {
				add_rule(server, key, value, CHECK_DUPLICATE_RULES);
			} else {
				add_rule(server, key, value, NO_FLAGS);
			}
			n--;
		}
		server->next_rule = NULL;
	} else if ((rawpkt[4] != 'E') && (rawpkt[4] != 'D') && (rawpkt[4] != 'm') && (rawpkt[4] != 'C') && show_errors) {
		/*	if ( pkt_count) { rawpkt-= 9; pktlen+= 9; } */
		fprintf(stderr, "Odd packet from HL server %s (packet len %d)\n", server->arg, pktlen);
		print_packet(server, rawpkt, pktlen);
	}

	return (DONE_AUTO);
}


query_status_t
deal_with_tribes_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	unsigned char *pkt, *end;
	int len, pnum, ping, packet_loss, n_teams, t;
	struct player *player;
	struct player **teams = NULL;
	struct player **last_player = &server->players;
	char buf[24];

	debug(2, "deal_with_tribes_packet %p, %d", server, pktlen);

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	} else {
		gettimeofday(&server->packet_time1, NULL);
	}

	if (pktlen < sizeof(tribes_info_reponse)) {
		return (PKT_ERROR);
	}

	if (strncmp(rawpkt, tribes_players_reponse, sizeof(tribes_players_reponse)) != 0) {
		return (PKT_ERROR);
	}

	pkt = (unsigned char *)&rawpkt[sizeof(tribes_info_reponse)];

	len = *pkt;     /* game name: "Tribes" */
	add_nrule(server, "gamename", (char *)pkt + 1, len);
	pkt += len + 1;
	len = *pkt;     /* version */
	add_nrule(server, "version", (char *)pkt + 1, len);
	pkt += len + 1;
	len = *pkt;     /* server name */
	server->server_name = strndup((char *)pkt + 1, len);
	pkt += len + 1;
	add_rule(server, "dedicated", *pkt ? "1" : "0", NO_FLAGS);
	pkt++;  /* flag: dedicated server */
	add_rule(server, "needpass", *pkt ? "1" : "0", NO_FLAGS);
	pkt++;  /* flag: password on server */
	server->num_players = *pkt++;
	server->max_players = *pkt++;

	sprintf(buf, "%u", (unsigned int)pkt[0] + (unsigned int)pkt[1] * 256);
	add_rule(server, "cpu", buf, NO_FLAGS);
	pkt++;          /* cpu speed, lsb */
	pkt++;          /* cpu speed, msb */

	len = *pkt;     /* Mod (game) */
	add_nrule(server, "mods", (char *)pkt + 1, len);
	pkt += len + 1;

	len = *pkt;     /* game (mission): "C&H" */
	add_nrule(server, "game", (char *)pkt + 1, len);
	pkt += len + 1;

	len = *pkt;     /* Mission (map) */
	server->map_name = strndup((char *)pkt + 1, len);
	pkt += len + 1;

	len = *pkt;     /* description (contains Admin: and Email: ) */
	debug(2, "%.*s\n", len, pkt + 1);
	pkt += len + 1;

	n_teams = *pkt++;       /* number of teams */
	if (n_teams == 255) {
		return (PKT_ERROR);
	}
	sprintf(buf, "%d", n_teams);
	add_rule(server, "numteams", buf, NO_FLAGS);

	len = *pkt;     /* first title */
	debug(2, "%.*s\n", len, pkt + 1);
	pkt += len + 1;

	len = *pkt;     /* second title */
	debug(2, "%.*s\n", len, pkt + 1);
	pkt += len + 1;

	if (n_teams > 1) {
		teams = (struct player **)calloc(1, sizeof(struct player *) * n_teams);
		for (t = 0; t < n_teams; t++) {
			teams[t] = (struct player *)calloc(1, sizeof(struct player));
			teams[t]->number = TRIBES_TEAM;
			teams[t]->team = t;
			len = *pkt;     /* team name */
			teams[t]->name = strndup((char *)pkt + 1, len);
			debug(2, "team#0 <%.*s>\n", len, pkt + 1);
			pkt += len + 1;

			len = *pkt;     /* team score */
			if (len > 2) {
				strncpy(buf, (char *)pkt + 1 + 3, len - 3);
				buf[len - 3] = '\0';
			} else {
				debug(2, "%s score len %d\n", server->arg, len);
				buf[0] = '\0';
			}
			teams[t]->frags = atoi(buf);
			debug(2, "team#0 <%.*s>\n", len - 3, pkt + 1 + 3);
			pkt += len + 1;
		}
	} else {
		len = *pkt;     /* DM team? */
		debug(2, "%.*s\n", len, pkt + 1);
		pkt += len + 1;
		pkt++;
		n_teams = 0;
	}

	pnum = 0;
	while ((char *)pkt < (rawpkt + pktlen)) {
		ping = (unsigned int)*pkt << 2;
		pkt++;
		packet_loss = *pkt;
		pkt++;
		debug(2, "player#%d, team #%d\n", pnum, (int)*pkt);
		pkt++;
		len = *pkt;
		if ((char *)pkt + len > (rawpkt + pktlen)) {
			break;
		}
		player = (struct player *)calloc(1, sizeof(struct player));
		player->team = pkt[-1];
		if (n_teams && (player->team < n_teams)) {
			player->team_name = teams[player->team]->name;
		} else if ((player->team == 255) && n_teams) {
			player->team_name = "Unknown";
		}
		player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
		player->ping = ping;
		player->packet_loss = packet_loss;
		player->name = strndup((char *)pkt + 1, len);
		debug(2, "player#%d, name %.*s\n", pnum, len, pkt + 1);
		pkt += len + 1;
		len = *pkt;
		debug(2, "player#%d, info <%.*s>\n", pnum, len, pkt + 1);
		end = (unsigned char *)strchr((char *)pkt + 9, 0x9);
		if (end) {
			strncpy(buf, (char *)pkt + 9, end - (pkt + 9));
			buf[end - (pkt + 9)] = '\0';
			player->frags = atoi(buf);
			debug(2, "player#%d, score <%.*s>\n", pnum, (unsigned)(end - (pkt + 9)), pkt + 9);
		}

		*last_player = player;
		last_player = &player->next;

		pkt += len + 1;
		pnum++;
	}

	for (t = n_teams; t; ) {
		t--;
		teams[t]->next = server->players;
		server->players = teams[t];
	}
	free(teams);

	return (DONE_AUTO);
}


void
get_tribes2_player_type(struct player *player)
{
	char *name = player->name;

	for ( ; *name; name++) {
		switch (*name) {
		case 0x8:
			player->type_flag = PLAYER_TYPE_NORMAL;
			continue;

		case 0xc:
			player->type_flag = PLAYER_TYPE_ALIAS;
			continue;

		case 0xe:
			player->type_flag = PLAYER_TYPE_BOT;
			continue;

		case 0xb:
			break;

		default:
			continue;
		}
		name++;
		if (isprint(*name)) {
			char *n = name;
			for ( ; isprint(*n); n++) {
			}
			player->tribe_tag = strndup(name, n - name);
			name = n;
		}
		if (!*name) {
			break;
		}
	}
}


query_status_t
deal_with_tribes2_packet(struct qserver *server, char *pkt, int pktlen)
{
	char str[256], *pktstart = pkt, *term, *start;
	unsigned int minimum_net_protocol, build_version, i, t, len, s, status;
	unsigned int net_protocol;
	unsigned short cpu_speed;
	int n_teams = 0, n_players;
	struct player **teams = NULL, *player;
	struct player **last_player = &server->players;
	int query_version;

	debug(2, "deal_with_tribes2_packet %p, %d", server, pktlen);

	pkt[pktlen] = '\0';

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	}

	/*
	 * else
	 * gettimeofday( &server->packet_time1, NULL);
	 */

	if (pkt[0] == TRIBES2_RESPONSE_PING) {
		if ((pkt[6] < 4) || (pkt[6] > 12) || (strncmp(pkt + 7, "VER", 3) != 0)) {
			return (PKT_ERROR);
		}

		strncpy(str, pkt + 10, pkt[6] - 3);
		str[pkt[6] - 3] = '\0';
		query_version = atoi(str);
		add_nrule(server, "queryversion", pkt + 7, pkt[6]);
		pkt += 7 + pkt[6];

		server->protocol_version = query_version;
		if ((query_version != 3) && (query_version != 5)) {
			server->server_name = strdup("Unknown query version");
			return (PKT_ERROR);
		}

		if (query_version == 5) {
			net_protocol = swap_long_from_little(pkt);
			sprintf(str, "%u", net_protocol);
			add_rule(server, "net_protocol", str, NO_FLAGS);
			pkt += 4;
		}
		minimum_net_protocol = swap_long_from_little(pkt);
		sprintf(str, "%u", minimum_net_protocol);
		add_rule(server, "minimum_net_protocol", str, NO_FLAGS);
		pkt += 4;
		build_version = swap_long_from_little(pkt);
		sprintf(str, "%u", build_version);
		add_rule(server, "build_version", str, NO_FLAGS);
		pkt += 4;

		server->server_name = strndup(pkt + 1, *(unsigned char *)(pkt));

		/* Always send the player request because the ping packet
		 * contains very little information */
		send_player_request_packet(server);
		return (0);
	} else if (pkt[0] != TRIBES2_RESPONSE_INFO) {
		return (PKT_ERROR);
	}

	pkt += 6;
	for (i = 0; i < *(unsigned char *)pkt; i++) {
		if (!isprint(pkt[i + 1])) {
			return (PKT_ERROR);
		}
	}
	add_nrule(server, server->type->game_rule, pkt + 1, *(unsigned char *)pkt);
	server->game = strndup(pkt + 1, *(unsigned char *)pkt);
	pkt += *pkt + 1;
	add_nrule(server, "mission", pkt + 1, *(unsigned char *)pkt);
	pkt += *pkt + 1;
	server->map_name = strndup(pkt + 1, *(unsigned char *)pkt);
	pkt += *pkt + 1;

	status = *(unsigned char *)pkt;
	sprintf(str, "%u", status);
	add_rule(server, "status", str, NO_FLAGS);
	if (status & TRIBES2_STATUS_DEDICATED) {
		add_rule(server, "dedicated", "1", NO_FLAGS);
	}
	if (status & TRIBES2_STATUS_PASSWORD) {
		add_rule(server, "password", "1", NO_FLAGS);
	}
	if (status & TRIBES2_STATUS_LINUX) {
		add_rule(server, "linux", "1", NO_FLAGS);
	}
	if (status & TRIBES2_STATUS_TEAMDAMAGE) {
		add_rule(server, "teamdamage", "1", NO_FLAGS);
	}
	if (server->protocol_version == 3) {
		if (status & TRIBES2_STATUS_TOURNAMENT_VER3) {
			add_rule(server, "tournament", "1", NO_FLAGS);
		}
		if (status & TRIBES2_STATUS_NOALIAS_VER3) {
			add_rule(server, "no_aliases", "1", NO_FLAGS);
		}
	} else {
		if (status & TRIBES2_STATUS_TOURNAMENT) {
			add_rule(server, "tournament", "1", NO_FLAGS);
		}
		if (status & TRIBES2_STATUS_NOALIAS) {
			add_rule(server, "no_aliases", "1", NO_FLAGS);
		}
	}
	pkt++;
	server->num_players = *(unsigned char *)pkt;
	pkt++;
	server->max_players = *(unsigned char *)pkt;
	pkt++;
	sprintf(str, "%u", *(unsigned char *)pkt);
	add_rule(server, "bot_count", str, NO_FLAGS);
	pkt++;
	cpu_speed = swap_short_from_little(pkt);
	sprintf(str, "%hu", cpu_speed);
	add_rule(server, "cpu_speed", str, NO_FLAGS);
	pkt += 2;

	if (strcmp(server->server_name, "VER3") == 0) {
		free(server->server_name);
		server->server_name = strndup(pkt + 1, *(unsigned char *)pkt);
	} else {
		add_nrule(server, "info", pkt + 1, *(unsigned char *)pkt);
	}

	pkt += *(unsigned char *)pkt + 1;
	len = swap_short_from_little(pkt);
	pkt += 2;
	start = pkt;
	if (len + (pkt - pktstart) > pktlen) {
		len -= (len + (pkt - pktstart)) - pktlen;
	}

	if ((len == 0) || (pkt - pktstart >= pktlen)) {
		goto info_done;
	}

	term = strchr(pkt, 0xa);
	if (!term) {
		goto info_done;
	}
	*term = '\0';
	n_teams = atoi(pkt);
	sprintf(str, "%d", n_teams);
	add_rule(server, "numteams", str, NO_FLAGS);
	pkt = term + 1;

	if (pkt - pktstart >= pktlen) {
		goto info_done;
	}

	teams = (struct player **)calloc(1, sizeof(struct player *) * n_teams);
	for (t = 0; t < n_teams; t++) {
		teams[t] = (struct player *)calloc(1, sizeof(struct player));
		teams[t]->number = TRIBES_TEAM;
		teams[t]->team = t;
		/* team name */
		term = strchr(pkt, 0x9);
		if (!term) {
			n_teams = t;
			goto info_done;
		}
		teams[t]->name = strndup(pkt, term - pkt);
		pkt = term + 1;
		term = strchr(pkt, 0xa);
		if (!term) {
			n_teams = t;
			goto info_done;
		}
		*term = '\0';
		teams[t]->frags = atoi(pkt);
		pkt = term + 1;
		if (pkt - pktstart >= pktlen) {
			goto info_done;
		}
	}

	term = strchr(pkt, 0xa);
	if (!term || (term - start >= len)) {
		goto info_done;
	}
	*term = '\0';
	n_players = atoi(pkt);
	pkt = term + 1;

	for (i = 0; i < n_players && pkt - start < len; i++) {
		pkt++;  /* skip first byte (0x10) */
		if (pkt - start >= len) {
			break;
		}
		player = (struct player *)calloc(1, sizeof(struct player));
		term = strchr(pkt, 0x11);
		if (!term || (term - start >= len)) {
			free(player);
			break;
		}
		player->name = strndup(pkt, term - pkt);
		get_tribes2_player_type(player);
		pkt = term + 1;
		pkt++;  /* skip 0x9 */
		if (pkt - start >= len) {
			break;
		}
		term = strchr(pkt, 0x9);
		if (!term || (term - start >= len)) {
			free(player->name);
			free(player);
			break;
		}
		for (t = 0; t < n_teams; t++) {
			if ((term - pkt == strlen(teams[t]->name)) && (strncmp(pkt, teams[t]->name, term - pkt) == 0)) {
				break;
			}
		}
		if (t == n_teams) {
			player->team = -1;
			player->team_name = "Unassigned";
		} else {
			player->team = t;
			player->team_name = teams[t]->name;
		}
		player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
		pkt = term + 1;
		for (s = 0; *pkt != 0xa && pkt - start < len; pkt++) {
			str[s++] = *pkt;
		}
		str[s] = '\0';
		player->frags = atoi(str);
		if (*pkt == 0xa) {
			pkt++;
		}

		*last_player = player;
		last_player = &player->next;
	}

info_done:
	for (t = n_teams; t; ) {
		t--;
		teams[t]->next = server->players;
		server->players = teams[t];
	}
	if (teams) {
		free(teams);
	}

	return (DONE_FORCE);
}


static const char GrPacketHead[] =
{
	'\xc0', '\xde', '\xf1', '\x11'
};
static char Dat2Reply1_2_10[] =
{
	'\xf4', '\x03', '\x14', '\x02', '\x0a', '\x41', '\x02', '\x0a', '\x41', '\x00', '\x00', '\x78', '\x30', '\x63'
};
static char Dat2Reply1_3[] =
{
	'\xf4', '\x03', '\x14', '\x03', '\x05', '\x41', '\x03', '\x05', '\x41', '\x00', '\x00', '\x78', '\x30', '\x63'
};
static char Dat2Reply1_4[] =
{
	'\xf4', '\x03', '\x14', '\x04', '\x00', '\x41', '\x04', '\x00', '\x41', '\x00', '\x00', '\x78', '\x30', '\x63'
};
//static char HDat2[]={'\xea','\x03','\x02','\x00','\x14'};

#define SHORT_GR_LEN		75
#define LONG_GR_LEN		500
#define UNKNOWN_VERSION		0
#define VERSION_1_2_10		1
#define VERSION_1_3		2
#define VERSION_1_4		3

query_status_t
deal_with_ghostrecon_packet(struct qserver *server, char *pkt, int pktlen)
{
	char str[256], StartFlag, *lpszIgnoreServerPlayer;
	char *lpszMission;
	unsigned int iIgnoreServerPlayer, iDedicatedServer, iUseStartTimer;
	unsigned short GrPayloadLen;
	int i;
	struct player *player;
	int iLen, iTemp;
	short sLen;
	int iSecsPlayed;
	long iSpawnType;
	int ServerVersion = UNKNOWN_VERSION;
	float flStartTimerSetPoint;

	debug(2, "deal_with_ghostrecon_packet %p, %d", server, pktlen);

	pkt[pktlen] = '\0';

	/*
	 * This function walks a packet that is recieved from a ghost recon server - default from port 2348. It does quite a few
	 * sanity checks along the way as the structure is not documented. The packet is mostly binary in nature with many string
	 * fields being variable in length, ie the length is listed foloowed by that many bytes. There are two structure arrays
	 * that have an array size followed by structure size * number of elements (player name and player data). This routine
	 * walks this packet and increments a pointer "pkt" to extract the info.
	 */

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	}

	/* sanity check against packet */
	if (memcmp(pkt, GrPacketHead, sizeof(GrPacketHead)) != 0) {
		server->server_name = strdup("Unknown Packet Header");
		return (PKT_ERROR);
	}

	pkt += sizeof(GrPacketHead);
	StartFlag = pkt[0];
	pkt += 1;
	if (StartFlag != 0x42) {
		server->server_name = strdup("Unknown Start Flag");
		return (PKT_ERROR);
	}

	/* compare packet length recieved to included size - header info */
	sLen = swap_short_from_little(pkt);
	pkt += 2;
	GrPayloadLen = pktlen - sizeof(GrPacketHead) - 3;
	// 3 = size slen + size start flag

	if (sLen != GrPayloadLen) {
		server->server_name = strdup("Packet Size Mismatch");
		return (PKT_ERROR);
	}

	/*
	 * Will likely need to verify and add to this "if" construct with every patch / add-on.
	 */
	if (memcmp(pkt, Dat2Reply1_2_10, sizeof(Dat2Reply1_2_10)) == 0) {
		ServerVersion = VERSION_1_2_10;
	} else if (memcmp(pkt, Dat2Reply1_3, sizeof(Dat2Reply1_3)) == 0) {
		ServerVersion = VERSION_1_3;
	} else if (memcmp(pkt, Dat2Reply1_4, sizeof(Dat2Reply1_4)) == 0) {
		ServerVersion = VERSION_1_4;
	}

	if (ServerVersion == UNKNOWN_VERSION) {
		server->server_name = strdup("Unknown GR Version");
		return (PKT_ERROR);
	}

	switch (ServerVersion) {
	case VERSION_1_2_10:
		strcpy(str, "1.2.10");
		pkt += sizeof(Dat2Reply1_2_10);
		break;

	case VERSION_1_3:
		strcpy(str, "1.3");
		pkt += sizeof(Dat2Reply1_3);
		break;

	case VERSION_1_4:
		strcpy(str, "1.4");
		pkt += sizeof(Dat2Reply1_4);
		break;
	}
	add_rule(server, "patch", str, NO_FLAGS);

	/* have player packet */

	// Ghost recon has one of the player slots filled up with the server program itself. By default we will
	// drop the first player listed. This causes a bit of a mess here and below but makes for the best display
	// a user can specify -grs,ignoreserverplayer=no to override this behaviour.

	lpszIgnoreServerPlayer = get_param_value(server, "ignoreserverplayer", "yes");
	for (i = 0; i < 4; i++) {
		str[i] = tolower(lpszIgnoreServerPlayer[i]);
	}
	if (strcmp(str, "yes") == 0) {
		iIgnoreServerPlayer = 1;
	} else {
		iIgnoreServerPlayer = 0;
	}

	pkt += 4;       /* unknown */

	// this is the first of many variable strings. get the length,
	// increment pointer over length, check for sanity,
	// get the string, increment the pointer over string (using length)

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		server->server_name = strdup("Server Name too Long");
		return (PKT_ERROR);
	}
	server->server_name = strndup(pkt, iLen);
	pkt += iLen;

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		add_rule(server, "error", "Map Name too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	server->map_name = strndup(pkt, iLen);
	pkt += iLen;

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		add_rule(server, "error", "Mission Name too Long", NO_FLAGS);
		return (PKT_ERROR);
	}

	/* mission does not make sense unless a coop game type. Since
	 * we dont know that now, we will save the mission and set
	 * the rule and free memory below when we know game type */
	lpszMission = strndup(pkt, iLen);
	pkt += iLen;

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		add_rule(server, "error", "Mission Type too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	add_nrule(server, "missiontype", pkt, iLen);
	pkt += iLen;

	if (pkt[1]) {
		add_rule(server, "password", "Yes", NO_FLAGS);
	} else {
		add_rule(server, "password", "No", NO_FLAGS);
	}
	pkt += 2;

	server->max_players = swap_long_from_little(pkt);
	pkt += 4;
	if (server->max_players > 36) {
		add_rule(server, "error", "Max players more then 36", NO_FLAGS);
		return (PKT_ERROR);
	}

	server->num_players = swap_long_from_little(pkt);
	pkt += 4;
	if (server->num_players > server->max_players) {
		add_rule(server, "error", "More then MAX Players", NO_FLAGS);
		return (PKT_ERROR);
	}

	if (iIgnoreServerPlayer) {
		// skip past first player
		server->num_players--;
		server->max_players--;
		iLen = swap_long_from_little(pkt);
		pkt += 4;
		pkt += iLen;
	}

	for (i = 0; i < server->num_players; i++) {
		// read each player name
		iLen = swap_long_from_little(pkt);
		pkt += 4;

		player = (struct player *)calloc(1, sizeof(struct player));

		if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
			add_rule(server, "error", "Player Name too Long", NO_FLAGS);
			return (PKT_ERROR);
		}
		player->name = strndup(pkt, iLen);
		pkt += iLen;            /* player name */
		player->team = i;       // tag so we can find this record when we have player dat.
		player->team_name = "Unassigned";
		player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
		player->frags = 0;

		player->next = server->players;
		server->players = player;
	}

	pkt += 17;

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		add_rule(server, "error", "Version too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	strncpy(str, pkt, iLen);
	add_rule(server, "version", str, NO_FLAGS);
	pkt += iLen;/* version */

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > LONG_GR_LEN)) {
		add_rule(server, "error", "Mods too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	server->game = strndup(pkt, iLen);

	for (i = 0; i < (int)strlen(server->game) - 5; i++) {
		// clean the "/mods/" part from every entry
		if (memcmp(&server->game[i], "\\mods\\", 6) == 0) {
			server->game[i] = ' ';
			strcpy(&server->game[i + 1], &server->game[i + 6]);
		}
	}
	add_rule(server, "game", server->game, NO_FLAGS);

	pkt += iLen;/* mods */

	iDedicatedServer = pkt[0];
	if (iDedicatedServer) {
		add_rule(server, "dedicated", "Yes", NO_FLAGS);
	} else {
		add_rule(server, "dedicated", "No", NO_FLAGS);
	}

	pkt += 1;       /* unknown */

	iSecsPlayed = swap_float_from_little(pkt);

	add_rule(server, "timeplayed", play_time(iSecsPlayed, 2), NO_FLAGS);

	pkt += 4;       /* time played */

	switch (pkt[0]) {
	case 3:
		strcpy(str, "Joining");
		break;

	case 4:
		strcpy(str, "Playing");
		break;

	case 5:
		strcpy(str, "Debrief");
		break;

	default:
		strcpy(str, "Undefined");
	}
	add_rule(server, "status", str, NO_FLAGS);

	pkt += 1;

	pkt += 3;       /* unknown */

	switch (pkt[0]) {
	case 2:
		strcpy(str, "COOP");
		break;

	case 3:
		strcpy(str, "SOLO");
		break;

	case 4:
		strcpy(str, "TEAM");
		break;

	default:
		sprintf(str, "UNKOWN %u", pkt[0]);
		break;
	}

	add_rule(server, "gamemode", str, NO_FLAGS);

	if (pkt[0] == 2) {
		add_rule(server, "mission", lpszMission, NO_FLAGS);
	} else {
		add_rule(server, "mission", "No Mission", NO_FLAGS);
	}

	free(lpszMission);

	pkt += 1;       /* Game Mode */

	pkt += 3;       /* unknown */

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > LONG_GR_LEN)) {
		add_rule(server, "error", "MOTD too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	strncpy(str, pkt, sizeof(str));
	str[sizeof(str) - 1] = 0;
	add_rule(server, "motd", str, NO_FLAGS);
	pkt += iLen;/* MOTD */

	iSpawnType = swap_long_from_little(pkt);

	switch (iSpawnType) {
	case 0:
		strcpy(str, "None");
		break;

	case 1:
		strcpy(str, "Individual");
		break;

	case 2:
		strcpy(str, "Team");
		break;

	case 3:
		strcpy(str, "Infinite");
		break;

	default:
		strcpy(str, "Unknown");
	}

	add_rule(server, "spawntype", str, NO_FLAGS);
	pkt += 4;       /* spawn type */

	iTemp = swap_float_from_little(pkt);
	add_rule(server, "gametime", play_time(iTemp, 2), NO_FLAGS);

	iTemp = iTemp - iSecsPlayed;

	if (iTemp <= 0) {
		iTemp = 0;
	}
	add_rule(server, "remainingtime", play_time(iTemp, 2), NO_FLAGS);
	pkt += 4;       /* Game time */

	iTemp = swap_long_from_little(pkt);
	if (iIgnoreServerPlayer) {
		iTemp--;
	}
	if (iTemp != server->num_players) {
		add_rule(server, "error", "Number of Players Mismatch", NO_FLAGS);
	}

	pkt += 4;       /* player count 2 */

	if (iIgnoreServerPlayer) {
		pkt += 5;       // skip first player data
	}

	for (i = 0; i < server->num_players; i++) {
		// for each player get binary data
		player = server->players;
		// first we must find the player - lets look for the tag
		while (player && (player->team != i)) {
			player = player->next;
		}
		/* get to player - linked list is in reverse order */

		if (player) {
			player->team = pkt[2];
			switch (player->team) {
			case 1:
				player->team_name = "Red";
				break;

			case 2:
				player->team_name = "Blue";
				break;

			case 3:
				player->team_name = "Yellow";
				break;

			case 4:
				player->team_name = "Green";
				break;

			case 5:
				player->team_name = "Unassigned";
				break;

			default:
				player->team_name = "Not Known";
				break;
			}
			player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
			player->deaths = pkt[1];
		}
		pkt += 5;       /* player data*/
	}

	for (i = 0; i < 5; i++) {
		pkt += 8;       /* team data who knows what they have in here */
	}

	pkt += 1;
	iUseStartTimer = pkt[0];// UseStartTimer

	pkt += 1;

	iTemp = flStartTimerSetPoint = swap_float_from_little(pkt);
	// Start Timer Set Point
	pkt += 4;

	if (iUseStartTimer) {
		add_rule(server, "usestarttime", "Yes", NO_FLAGS);
		add_rule(server, "starttimeset", play_time(iTemp, 2), NO_FLAGS);
	} else {
		add_rule(server, "usestarttime", "No", NO_FLAGS);
		add_rule(server, "starttimeset", play_time(0, 2), NO_FLAGS);
	}

	if ((ServerVersion == VERSION_1_3) ||           // stuff added in patch 1.3
	    (ServerVersion == VERSION_1_4)) {
		iTemp = swap_float_from_little(pkt);    // Debrief Time
		add_rule(server, "debrieftime", play_time(iTemp, 2), NO_FLAGS);
		pkt += 4;

		iTemp = swap_float_from_little(pkt);// Respawn Min
		add_rule(server, "respawnmin", play_time(iTemp, 2), NO_FLAGS);
		pkt += 4;

		iTemp = swap_float_from_little(pkt);// Respawn Max
		add_rule(server, "respawnmax", play_time(iTemp, 2), NO_FLAGS);
		pkt += 4;

		iTemp = swap_float_from_little(pkt);// Respawn Invulnerable
		add_rule(server, "respawnsafe", play_time(iTemp, 2), NO_FLAGS);
		pkt += 4;
	} else {
		add_rule(server, "debrieftime", "Undefined", NO_FLAGS);
		add_rule(server, "respawnmin", "Undefined", NO_FLAGS);
		add_rule(server, "respawnmax", "Undefined", NO_FLAGS);
		add_rule(server, "respawnsafe", "Undefined", NO_FLAGS);
	}

	pkt += 4;       // 4
	iTemp = pkt[0]; // Spawn Count

	if ((iSpawnType == 1) || (iSpawnType == 2)) {
		/* Individual or team */
		sprintf(str, "%u", iTemp);
	} else {
		/* else not used */
		sprintf(str, "%u", 0);
	}
	add_rule(server, "spawncount", str, NO_FLAGS);
	pkt += 1;       // 5

	pkt += 4;       // 9

	iTemp = pkt[0]; // Allow Observers
	if (iTemp) {
		strcpy(str, "Yes");
	} else {
		/* else not used */
		strcpy(str, "No");
	}
	add_rule(server, "allowobservers", str, NO_FLAGS);
	pkt += 1;       // 10

	pkt += 3;       // 13

	//	pkt += 13;

	if (iUseStartTimer) {
		iTemp = swap_float_from_little(pkt);// Start Timer Count
		add_rule(server, "startwait", play_time(iTemp, 2), NO_FLAGS);
	} else {
		add_rule(server, "startwait", play_time(0, 2), NO_FLAGS);
	}
	pkt += 4;       //17

	iTemp = pkt[0]; // IFF
	switch (iTemp) {
	case 0:
		strcpy(str, "None");
		break;

	case 1:
		strcpy(str, "Reticule");
		break;

	case 2:
		strcpy(str, "Names");
		break;

	default:
		strcpy(str, "Unknown");
		break;
	}
	add_rule(server, "iff", str, NO_FLAGS);
	pkt += 1;       // 18

	iTemp = pkt[0]; // Threat Indicator
	if (iTemp) {
		add_rule(server, "ti", "ON ", NO_FLAGS);
	} else {
		add_rule(server, "ti", "OFF", NO_FLAGS);
	}

	pkt += 1;       // 19

	pkt += 5;       // 24

	iLen = swap_long_from_little(pkt);
	pkt += 4;
	if ((iLen < 1) || (iLen > SHORT_GR_LEN)) {
		add_rule(server, "error", "Restrictions too Long", NO_FLAGS);
		return (PKT_ERROR);
	}
	add_rule(server, "restrict", pkt, NO_FLAGS);
	pkt += iLen;/* restrictions */

	pkt += 23;

	/*
	 * if ( ghostrecon_debug) print_packet( pkt, GrPayloadLen);
	 */

	return (DONE_FORCE);
}


char *
find_ravenshield_game(char *gameno)
{
	switch (atoi(gameno)) {
	case 8:
		return (strdup("Team Deathmatch"));

		break;

	case 13:
		return (strdup("Deathmatch"));

		break;

	case 14:
		return (strdup("Team Deathmatch"));

		break;

	case 15:
		return (strdup("Bomb"));

		break;

	case 16:
		return (strdup("Escort Pilot"));

		break;

	default:
		// 1.50 and above actually uses a string so
		// return that
		return (strdup(gameno));

		break;
	}
}


char *
find_savage_game(char *gametype)
{
	if (0 == strcmp("RTSS", gametype)) {
		return (strdup("RTSS"));
	} else {
		return (strdup("Unknown"));
	}
}


query_status_t
deal_with_ravenshield_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, *key, *value;

	debug(2, "deal_with_ravenshield_packet %p, %d", server, pktlen);

	server->n_servers++;
	if (NULL == server->server_name) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	} else {
		gettimeofday(&server->packet_time1, NULL);
	}

	rawpkt[pktlen] = '\0';

	s = rawpkt;
	while (*s) {
		// Find the seperator
		while (*s && *s != '\xB6') {
			s++;
		}

		if (!*s) {
			// Hit the end no more
			break;
		}

		// key start
		key = ++s;
		while (*s && *s != ' ') {
			s++;
		}
		if (*s != ' ') {
			// malformed
			break;
		}
		*s++ = '\0';
		// key end
		// value start
		value = s;

		while (*s && *s != '\xB6') {
			s++;
		}

		if (*s == '\xB6') {
			*(s - 1) = '\0';
		}

		// Decode current key par
		if (0 == strcmp("A1", key)) {
			// Max players
			server->max_players = atoi(value);
		} else if (0 == strcmp("A2", key)) {
			// TeamKillerPenalty
			add_rule(server, "TeamKillerPenalty", value, NO_FLAGS);
		} else if (0 == strcmp("B1", key)) {
			// Current players
			server->num_players = atoi(value);
		} else if (0 == strcmp("B2", key)) {
			// AllowRadar
			add_rule(server, "AllowRadar", value, NO_FLAGS);
		} else if (0 == strcmp("D2", key)) {
			// Version info
			add_rule(server, "Version", value, NO_FLAGS);
		} else if (0 == strcmp("E1", key)) {
			// Current map
			server->map_name = strdup(value);
		} else if (0 == strcmp("E2", key)) {
			// Unknown
		} else if (0 == strcmp("F1", key)) {
			// Game type
			server->game = find_ravenshield_game(value);
			add_rule(server, server->type->game_rule, server->game, NO_FLAGS);
		} else if (0 == strcmp("F2", key)) {
			// Unknown
		} else if (0 == strcmp("G1", key)) {
			// Password
			add_rule(server, "Password", value, NO_FLAGS);
		} else if (0 == strcmp("G2", key)) {
			// Query port
		} else if (0 == strcmp("H1", key)) {
			// Unknown
		} else if (0 == strcmp("H2", key)) {
			// Number of Terrorists
			add_rule(server, "nbTerro", value, NO_FLAGS);
		} else if (0 == strcmp("I1", key)) {
			// Server name
			server->server_name = strdup(value);
		} else if (0 == strcmp("I2", key)) {
			// Unknown
		} else if (0 == strcmp("J1", key)) {
			// Game Type Order
			// Not pretty ignore for now
			//add_rule( server, "Game Type Order", value, NO_FLAGS );
		} else if (0 == strcmp("J2", key)) {
			// RotateMap
			add_rule(server, "RotateMap", value, NO_FLAGS);
		} else if (0 == strcmp("K1", key)) {
			// Map Cycle
			// Not pretty ignore for now
			//add_rule( server, "Map Cycle", value, NO_FLAGS );
		} else if (0 == strcmp("K2", key)) {
			// Force First Person Weapon
			add_rule(server, "ForceFPersonWeapon", value, NO_FLAGS);
		} else if (0 == strcmp("L1", key)) {
			// Players names
			int player_number = 0;
			char *n = value;

			if (*n == '/') {
				// atleast 1 player
				n++;
				while (*n && *n != '\xB6') {
					char *player_name = n;
					while (*n && *n != '/' && *n != '\xB6') {
						n++;
					}

					if (*n == '/') {
						*n++ = '\0';
					} else if (*n == '\xB6') {
						*(n - 1) = '\0';
					}

					if (0 != strlen(player_name)) {
						struct player *player = add_player(server, player_number);
						if (NULL != player) {
							player->name = strdup(player_name);
						}
						player_number++;
					}
				}
			}
		} else if (0 == strcmp("L3", key)) {
			// PunkBuster state
			add_rule(server, "PunkBuster", value, NO_FLAGS);
		} else if (0 == strcmp("M1", key)) {
			// Players times
			int player_number = 0;
			char *n = value;
			if (*n == '/') {
				// atleast 1 player
				n++;
				while (*n && *n != '\xB6') {
					char *time = n;
					while (*n && *n != '/' && *n != '\xB6') {
						n++;
					}

					if (*n == '/') {
						*n++ = '\0';
					} else if (*n == '\xB6') {
						*(n - 1) = '\0';
					}

					if (0 != strlen(time)) {
						int mins, seconds;
						if (2 == sscanf(time, "%d:%d", &mins, &seconds)) {
							struct player *player = get_player_by_number(server, player_number);
							if (NULL != player) {
								player->connect_time = mins * 60 + seconds;
							}
						}
						player_number++;
					}
				}
			}
		} else if (0 == strcmp("N1", key)) {
			// Players ping
			int player_number = 0;
			char *n = value;
			if (*n == '/') {
				// atleast 1 player
				n++;
				while (*n && *n != '\xB6') {
					char *ping = n;
					while (*n && *n != '/' && *n != '\xB6') {
						n++;
					}

					if (*n == '/') {
						*n++ = '\0';
					} else if (*n == '\xB6') {
						*(n - 1) = '\0';
					}

					if (0 != strlen(ping)) {
						struct player *player = get_player_by_number(server, player_number);
						if (NULL != player) {
							player->ping = atoi(ping);
						}
						player_number++;
					}
				}
			}
		} else if (0 == strcmp("O1", key)) {
			// Players fags
			int player_number = 0;
			char *n = value;
			if (*n == '/') {
				// atleast 1 player
				n++;
				while (*n && *n != '\xB6') {
					char *frags = n;
					while (*n && *n != '/' && *n != '\xB6') {
						n++;
					}

					if (*n == '/') {
						*n++ = '\0';
					} else if (*n == '\xB6') {
						*(n - 1) = '\0';
					}

					if (0 != strlen(frags)) {
						struct player *player = get_player_by_number(server, player_number);
						if (NULL != player) {
							player->frags = atoi(frags);
						}
						player_number++;
					}
				}
			}
		} else if (0 == strcmp("P1", key)) {
			// Game port
			// Not pretty ignore for now

			/*
			 * change_server_port( server, atoi( value ), 0 );
			 */
		} else if (0 == strcmp("Q1", key)) {
			// RoundsPerMatch
			add_rule(server, "RoundsPerMatch", value, NO_FLAGS);
		} else if (0 == strcmp("R1", key)) {
			// RoundTime
			add_rule(server, "RoundTime", value, NO_FLAGS);
		} else if (0 == strcmp("S1", key)) {
			// BetweenRoundTime
			add_rule(server, "BetweenRoundTime", value, NO_FLAGS);
		} else if (0 == strcmp("T1", key)) {
			// BombTime
			add_rule(server, "BombTime", value, NO_FLAGS);
		} else if (0 == strcmp("W1", key)) {
			// ShowNames
			add_rule(server, "ShowNames", value, NO_FLAGS);
		} else if (0 == strcmp("X1", key)) {
			// InternetServer
			add_rule(server, "InternetServer", value, NO_FLAGS);
		} else if (0 == strcmp("Y1", key)) {
			// FriendlyFire
			add_rule(server, "FriendlyFire", value, NO_FLAGS);
		} else if (0 == strcmp("Z1", key)) {
			// Autobalance
			add_rule(server, "Autobalance", value, NO_FLAGS);
		}
	}

	return (DONE_FORCE);
}


query_status_t
deal_with_savage_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, *key, *value, *end;

	debug(2, "deal_with_savage_packet %p, %d", server, pktlen);

	server->n_servers++;
	if (NULL == server->server_name) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	} else {
		gettimeofday(&server->packet_time1, NULL);
	}

	rawpkt[pktlen] = '\0';

	end = s = rawpkt;
	end += pktlen;
	while (*s) {
		// Find the seperator
		while (s <= end && *s != '\xFF') {
			s++;
		}

		if (s >= end) {
			// Hit the end no more
			break;
		}

		// key start
		key = ++s;
		while (s < end && *s != '\xFE') {
			s++;
		}
		if (*s != '\xFE') {
			// malformed
			break;
		}
		*s++ = '\0';
		// key end
		// value start
		value = s;

		while (s < end && *s != '\xFF') {
			s++;
		}

		if (*s == '\xFF') {
			*s = '\0';
		}
		//fprintf( stderr, "'%s' = '%s'\n", key, value );

		// Decode current key par
		if (0 == strcmp("cmax", key)) {
			// Max players
			server->max_players = atoi(value);
		} else if (0 == strcmp("cnum", key)) {
			// Current players
			server->num_players = atoi(value);
		} else if (0 == strcmp("bal", key)) {
			// Balance
			add_rule(server, "Balance", value, NO_FLAGS);
		} else if (0 == strcmp("world", key)) {
			// Current map
			server->map_name = strdup(value);
		} else if (0 == strcmp("gametype", key)) {
			// Game type
			server->game = find_savage_game(value);
			add_rule(server, server->type->game_rule, server->game, NO_FLAGS);
		} else if (0 == strcmp("pure", key)) {
			// Pure
			add_rule(server, "Pure", value, NO_FLAGS);
		} else if (0 == strcmp("time", key)) {
			// Current game time
			add_rule(server, "Time", value, NO_FLAGS);
		} else if (0 == strcmp("notes", key)) {
			// Notes
			add_rule(server, "Notes", value, NO_FLAGS);
		} else if (0 == strcmp("needcmdr", key)) {
			// Need Commander
			add_rule(server, "Need Commander", value, NO_FLAGS);
		} else if (0 == strcmp("name", key)) {
			// Server name
			server->server_name = strdup(value);
		} else if (0 == strcmp("fw", key)) {
			// Firewalled
			add_rule(server, "Firewalled", value, NO_FLAGS);
		} else if (0 == strcmp("players", key)) {
			// Players names
			int player_number = 0;
			int team_number = 1;
			char *team_name, *player_name, *n;
			n = team_name = value;

			// team name
			n++;
			while (*n && *n != '\x0a') {
				n++;
			}

			if (*n != '\x0a') {
				// Broken data
				break;
			}
			*n = '\0';

			player_name = ++n;
			while (*n) {
				while (*n && *n != '\x0a') {
					n++;
				}

				if (*n != '\x0a') {
					// Broken data
					break;
				}
				*n = '\0';
				n++;

				if (0 == strncmp("Team ", player_name, 5)) {
					team_name = player_name;
					team_number++;
				} else {
					if (0 != strlen(player_name)) {
						struct player *player = add_player(server, player_number);
						if (NULL != player) {
							player->name = strdup(player_name);
							player->team = team_number;
							player->team_name = strdup(team_name);
						}
						player_number++;
					}
				}
				player_name = n;
			}
		}

		*s = '\xFF';
	}

	return (DONE_FORCE);
}


query_status_t
deal_with_farcry_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *s, *key, *value, *end;

	debug(2, "deal_with_farcry_packet %p, %d", server, pktlen);

	server->n_servers++;
	if (NULL == server->server_name) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	} else {
		gettimeofday(&server->packet_time1, NULL);
	}

	rawpkt[pktlen] = '\0';

	end = s = rawpkt;
	end += pktlen;
	while (*s) {
		// Find the seperator
		while (s <= end && *s != '\xFF') {
			s++;
		}

		if (s >= end) {
			// Hit the end no more
			break;
		}

		// key start
		key = ++s;
		while (s < end && *s != '\xFE') {
			s++;
		}
		if (*s != '\xFE') {
			// malformed
			break;
		}
		*s++ = '\0';
		// key end
		// value start
		value = s;

		while (s < end && *s != '\xFF') {
			s++;
		}

		if (*s == '\xFF') {
			*s = '\0';
		}
		//fprintf( stderr, "'%s' = '%s'\n", key, value );

		// Decode current key par
		if (0 == strcmp("cmax", key)) {
			// Max players
			server->max_players = atoi(value);
		} else if (0 == strcmp("cnum", key)) {
			// Current players
			server->num_players = atoi(value);
		} else if (0 == strcmp("bal", key)) {
			// Balance
			add_rule(server, "Balance", value, NO_FLAGS);
		} else if (0 == strcmp("world", key)) {
			// Current map
			server->map_name = strdup(value);
		} else if (0 == strcmp("gametype", key)) {
			// Game type
			server->game = find_savage_game(value);
			add_rule(server, server->type->game_rule, server->game, NO_FLAGS);
		} else if (0 == strcmp("pure", key)) {
			// Pure
			add_rule(server, "Pure", value, NO_FLAGS);
		} else if (0 == strcmp("time", key)) {
			// Current game time
			add_rule(server, "Time", value, NO_FLAGS);
		} else if (0 == strcmp("notes", key)) {
			// Notes
			add_rule(server, "Notes", value, NO_FLAGS);
		} else if (0 == strcmp("needcmdr", key)) {
			// Need Commander
			add_rule(server, "Need Commander", value, NO_FLAGS);
		} else if (0 == strcmp("name", key)) {
			// Server name
			server->server_name = strdup(value);
		} else if (0 == strcmp("fw", key)) {
			// Firewalled
			add_rule(server, "Firewalled", value, NO_FLAGS);
		} else if (0 == strcmp("players", key)) {
			// Players names
			int player_number = 0;
			int team_number = 1;
			char *team_name, *player_name, *n;
			n = team_name = value;

			// team name
			n++;
			while (*n && *n != '\x0a') {
				n++;
			}

			if (*n != '\x0a') {
				// Broken data
				break;
			}
			*n = '\0';

			player_name = ++n;
			while (*n) {
				while (*n && *n != '\x0a') {
					n++;
				}

				if (*n != '\x0a') {
					// Broken data
					break;
				}
				*n = '\0';
				n++;

				if (0 == strncmp("Team ", player_name, 5)) {
					team_name = player_name;
					team_number++;
				} else {
					if (0 != strlen(player_name)) {
						struct player *player = add_player(server, player_number);
						if (NULL != player) {
							player->name = strdup(player_name);
							player->team = team_number;
							player->team_name = strdup(team_name);
						}
						player_number++;
					}
				}
				player_name = n;
			}
		}

		*s = '\xFF';
	}

	return (DONE_FORCE);
}


/* postions of map name, player name (in player substring), zero-based */
#define BFRIS_MAP_POS		18
#define BFRIS_PNAME_POS		11
query_status_t
deal_with_bfris_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	int i, player_data_pos, nplayers;
	SavedData *sdata;
	unsigned char *saved_data;
	int saved_data_size;

	debug(2, "deal_with_bfris_packet %p, %d", server, pktlen);

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	/* add to the data previously saved */
	sdata = &server->saved_data;
	if (!sdata->data) {
		sdata->data = (char *)malloc(pktlen);
	} else {
		sdata->data = (char *)realloc(sdata->data, sdata->datalen + pktlen);
	}

	memcpy(sdata->data + sdata->datalen, rawpkt, pktlen);
	sdata->datalen += pktlen;

	saved_data = (unsigned char *)sdata->data;
	saved_data_size = sdata->datalen;

	/* after we get the server portion of the data, server->game != NULL */
	if (!server->game) {
		/* server data goes up to map name */
		if (sdata->datalen <= BFRIS_MAP_POS) {
			return (INPROGRESS);
		}

		/* see if map name is complete */
		player_data_pos = 0;
		for (i = BFRIS_MAP_POS; i < saved_data_size; i++) {
			if (saved_data[i] == '\0') {
				player_data_pos = i + 1;
				/* data must extend beyond map name */
				if (saved_data_size <= player_data_pos) {
					return (INPROGRESS);
				}
				break;
			}
		}

		/* did we find beginning of player data? */
		if (!player_data_pos) {
			return (INPROGRESS);
		}

		/* now we can go ahead and fill in server data */
		server->map_name = strdup((char *)saved_data + BFRIS_MAP_POS);
		server->max_players = saved_data[12];
		server->protocol_version = saved_data[11];

		/* save game type */
		switch (saved_data[13] & 15) {
		case 0:
			server->game = "FFA";
			break;

		case 5:
			server->game = "Rover";
			break;

		case 6:
			server->game = "Occupation";
			break;

		case 7:
			server->game = "SPAAL";
			break;

		case 8:
			server->game = "CTF";
			break;

		default:
			server->game = "unknown";
			break;
		}
		server->flags |= FLAG_DO_NOT_FREE_GAME;
		add_rule(server, server->type->game_rule, server->game, NO_FLAGS);

		if (get_server_rules) {
			char buf[24];

			/* server revision */
			sprintf(buf, "%d", (unsigned int)saved_data[11]);
			add_rule(server, "Revision", buf, NO_FLAGS);

			/* latency */
			sprintf(buf, "%d", (unsigned int)saved_data[10]);
			add_rule(server, "Latency", buf, NO_FLAGS);

			/* player allocation */
			add_rule(server, "Allocation", saved_data[13] & 16 ? "Automatic" : "Manual", NO_FLAGS);
		}
	}

	/* If we got this far, we know the data saved goes at least to the start of
	 * the player information, and that the server data is taken care of.
	 */

	/* start of player data */
	player_data_pos = BFRIS_MAP_POS + strlen((char *)saved_data + BFRIS_MAP_POS) + 1;

	/* ensure all player data have arrived */
	nplayers = 0;
	while (saved_data[player_data_pos] != '\0') {
		player_data_pos += BFRIS_PNAME_POS;

		/* does player data extend to player name? */
		if (saved_data_size <= player_data_pos + 1) {
			return (INPROGRESS);
		}

		/* does player data extend to end of player name? */
		for (i = 0; player_data_pos + i < saved_data_size; i++) {
			if (saved_data_size == player_data_pos + i + 1) {
				return (INPROGRESS);
			}

			if (saved_data[player_data_pos + i] == '\0') {
				player_data_pos += i + 1;
				nplayers++;
				break;
			}
		}
	}
	/* all player data are complete */

	server->num_players = nplayers;

	if (get_player_info) {
		/* start of player data */
		player_data_pos = BFRIS_MAP_POS + strlen((char *)saved_data + BFRIS_MAP_POS) + 1;

		for (i = 0; i < nplayers; i++) {
			struct player *player;
			player = add_player(server, saved_data[player_data_pos]);

			player->ship = saved_data[player_data_pos + 1];
			player->ping = saved_data[player_data_pos + 2];
			player->frags = saved_data[player_data_pos + 3];
			player->team = saved_data[player_data_pos + 4];
			switch (player->team) {
			case 0:
				player->team_name = "silver";
				break;

			case 1:
				player->team_name = "red";
				break;

			case 2:
				player->team_name = "blue";
				break;

			case 3:
				player->team_name = "green";
				break;

			case 4:
				player->team_name = "purple";
				break;

			case 5:
				player->team_name = "yellow";
				break;

			case 6:
				player->team_name = "cyan";
				break;

			default:
				player->team_name = "unknown";
				break;
			}
			player->flags |= PLAYER_FLAG_DO_NOT_FREE_TEAM;
			player->room = saved_data[player_data_pos + 5];

			/* score is little-endian integer */
			player->score = saved_data[player_data_pos + 7] +
			    (saved_data[player_data_pos + 8] << 8) +
			    (saved_data[player_data_pos + 9] << 16) +
			    (saved_data[player_data_pos + 10] << 24);

			/* for archs with > 4-byte int */
			if (player->score & 0x80000000) {
				player->score = -(~(player->score)) - 1;
			}

			player_data_pos += BFRIS_PNAME_POS;
			player->name = strdup((char *)saved_data + player_data_pos);

			player_data_pos += strlen(player->name) + 1;
		}
	}

	server->server_name = BFRIS_SERVER_NAME;

	return (DONE_FORCE);
}


struct rule *
add_uchar_rule(struct qserver *server, char *key, unsigned char value)
{
	char buf[24];

	sprintf(buf, "%u", (unsigned)value);
	return (add_rule(server, key, buf, NO_FLAGS));
}


query_status_t
deal_with_descent3_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *pkt;
	char buf[24];

	debug(2, "deal_with_descent3_packet %p, %d", server, pktlen);

	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	}

	if (pktlen < 4) {
		fprintf(stderr, "short descent3 packet\n");
		print_packet(server, rawpkt, pktlen);
		return (PKT_ERROR);
	}

	/* 'info' response */
	if (rawpkt[1] == 0x1f) {
		if (server->server_name != NULL) {
			return (PKT_ERROR);
		}

		pkt = &rawpkt[0x15];
		server->server_name = strdup(pkt);
		pkt += strlen(pkt) + 2;
		server->map_name = strdup(pkt); /* mission name (blah.mn3) */
		pkt += strlen(pkt) + 2;
		add_rule(server, "level_name", pkt, NO_FLAGS);
		pkt += strlen(pkt) + 2;
		add_rule(server, "gametype", pkt, NO_FLAGS);
		pkt += strlen(pkt) + 1;

		sprintf(buf, "%hu", swap_short_from_little(pkt));
		add_rule(server, "level_num", buf, NO_FLAGS);
		pkt += 2;
		server->num_players = swap_short_from_little(pkt);
		pkt += 2;
		server->max_players = swap_short_from_little(pkt);
		pkt += 2;

		/* unknown/undecoded fields.. stuff like permissible, banned items/ships, etc */
		add_uchar_rule(server, "u0", pkt[0]);
		add_uchar_rule(server, "u1", pkt[1]);
		add_uchar_rule(server, "u2", pkt[2]);
		add_uchar_rule(server, "u3", pkt[3]);
		add_uchar_rule(server, "u4", pkt[4]);
		add_uchar_rule(server, "u5", pkt[5]);
		add_uchar_rule(server, "u6", pkt[6]);
		add_uchar_rule(server, "u7", pkt[7]);
		add_uchar_rule(server, "u8", pkt[8]);

		add_uchar_rule(server, "randpowerup", (unsigned char)!(pkt[4] & 1));/*
		                                                                     *                                                                     randomize powerup spawn */
		add_uchar_rule(server, "acccollisions", (unsigned char)((pkt[5] & 4) > 0));
		/* accurate collision detection */
		add_uchar_rule(server, "brightships", (unsigned char)((pkt[5] & 16) > 0));
		/* bright player ships */
		add_uchar_rule(server, "mouselook", (unsigned char)((pkt[6] & 1) > 0)); /*
		                                                                         *                                                                         mouselook enabled */
		sprintf(buf, "%s%s", (pkt[4] & 16) ? "PP" : "CS", (pkt[6] & 1) ? "-ML" : "");
		add_rule(server, "servertype", buf, NO_FLAGS);

		sprintf(buf, "%hhu", pkt[9]);
		add_rule(server, "difficulty", buf, NO_FLAGS);

		/* unknown/undecoded fields after known flags removed */
		add_uchar_rule(server, "x4", (unsigned char)(pkt[4] & ~(1 + 16)));
		add_uchar_rule(server, "x5", (unsigned char)(pkt[5] & ~(4 + 16)));
		add_uchar_rule(server, "x6", (unsigned char)(pkt[6] & ~1));

		if (get_player_info && server->num_players) {
			server->next_player_info = 0;
			send_player_request_packet(server);
			return (INPROGRESS);
		}
	}
	/* MP_PLAYERLIST_DATA */
	else if (rawpkt[1] == 0x73) {
		struct player *player;
		struct player **last_player = &server->players;

		if (server->players != NULL) {
			return (PKT_ERROR);
		}

		pkt = &rawpkt[0x4];
		while (*pkt) {
			player = (struct player *)calloc(1, sizeof(struct player));
			player->name = strdup(pkt);
			pkt += strlen(pkt) + 1;
			*last_player = player;
			last_player = &player->next;
		}
		server->next_player_info = NO_PLAYER_INFO;
	} else {
		fprintf(stderr, "unknown d3 packet\n");
		print_packet(server, rawpkt, pktlen);
	}

	return (DONE_FORCE);
}


#define EYE_NAME_MASK		1
#define EYE_TEAM_MASK		2
#define EYE_SKIN_MASK		4
#define EYE_SCORE_MASK		8
#define EYE_PING_MASK		16
#define EYE_TIME_MASK		32

query_status_t
deal_with_eye_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *next, *end, *value, *key;
	struct player **last_player;
	unsigned char pkt_index, pkt_max;
	unsigned int pkt_id;

	debug(2, "deal_with_eye_packet %p, %d", server, pktlen);

	if (pktlen < 4) {
		return (PKT_ERROR);
	}

	if ((rawpkt[0] != 'E') || (rawpkt[1] != 'Y') || (rawpkt[2] != 'E')) {
		return (PKT_ERROR);
	}

	server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);

	end = rawpkt + pktlen;
	pkt_index = rawpkt[3] - '0';

	if ((pktlen == 1364) || (pkt_index != 1)) {
		/* fragmented packet */
		SavedData *sdata;

		/* EYE doesn't tell us how many packets to expect. Two packets
		 * is enough for 100+ players on a BF1942 server with standard
		 * server rules.
		 */
		pkt_max = 2;
		memcpy(&pkt_id, &rawpkt[pktlen - 4], 4);

		if (server->saved_data.data == NULL) {
			sdata = &server->saved_data;
		} else {
			sdata = (SavedData *)calloc(1, sizeof(SavedData));
			sdata->next = server->saved_data.next;
			server->saved_data.next = sdata;
		}

		sdata->pkt_index = pkt_index - 1;
		sdata->pkt_max = pkt_max;
		sdata->pkt_id = pkt_id;
		if (pkt_index == 1) {
			sdata->datalen = pktlen - 4;
		} else {
			sdata->datalen = pktlen - 8;
		}
		sdata->data = (char *)malloc(sdata->datalen);

		if (NULL == sdata->data) {
			return (MEM_ERROR);
		}

		if (pkt_index == 1) {
			memcpy(sdata->data, &rawpkt[0], sdata->datalen);
		} else {
			memcpy(sdata->data, &rawpkt[4], sdata->datalen);
		}

		/* combine_packets will call us recursively */
		return (combine_packets(server));
	}

	value = dup_n1string(&rawpkt[4], end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	add_rule(server, "gamename", value, NO_VALUE_COPY);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	add_rule(server, "hostport", value, NO_VALUE_COPY);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	server->server_name = value;

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	server->game = value;
	add_rule(server, server->type->game_rule, value, NO_FLAGS);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	server->map_name = value;

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	add_rule(server, "_version", value, NO_VALUE_COPY);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	add_rule(server, "_password", value, NO_VALUE_COPY);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	server->num_players = atoi(value);
	free(value);

	value = dup_n1string(next, end, &next);
	if (value == NULL) {
		return (MEM_ERROR);
	}
	server->max_players = atoi(value);
	free(value);

	/* rule1,value1,rule2,value2, ... empty string */

	do {
		key = dup_n1string(next, end, &next);
		if (key == NULL) {
			break;
		} else if (key[0] == '\0') {
			free(key);
			break;
		}

		value = dup_n1string(next, end, &next);
		if (value == NULL) {
			free(key);
			break;
		}

		add_rule(server, key, value, NO_VALUE_COPY | NO_KEY_COPY);
	} while (1);

	/* [mask1]<name1><team1><skin1><score1><ping1><time1>[mask2]... */

	last_player = &server->players;
	while (next && next < end) {
		struct player *player;
		unsigned mask = *((unsigned char *)next);
		next++;
		if (next >= end) {
			break;
		}
		if (mask == 0) {
			break;
		}
		player = (struct player *)calloc(1, sizeof(struct player));
		if (player == NULL) {
			break;
		}
		if (mask & EYE_NAME_MASK) {
			player->name = dup_n1string(next, end, &next);
			//fprintf( stderr, "Player '%s'\n", player->name );
			if (player->name == NULL) {
				break;
			}
		}
		if (mask & EYE_TEAM_MASK) {
			value = dup_n1string(next, end, &next);
			if (value == NULL) {
				break;
			}
			if (isdigit((unsigned char)value[0])) {
				player->team = atoi(value);
				free(value);
			} else {
				player->team_name = value;
			}
		}
		if (mask & EYE_SKIN_MASK) {
			player->skin = dup_n1string(next, end, &next);
			if (player->skin == NULL) {
				break;
			}
		}
		if (mask & EYE_SCORE_MASK) {
			value = dup_n1string(next, end, &next);
			if (value == NULL) {
				break;
			}
			player->score = atoi(value);
			player->frags = player->score;
			free(value);
		}
		if (mask & EYE_PING_MASK) {
			value = dup_n1string(next, end, &next);
			if (value == NULL) {
				break;
			}
			player->ping = atoi(value);
			free(value);
		}
		if (mask & EYE_TIME_MASK) {
			value = dup_n1string(next, end, &next);
			if (value == NULL) {
				break;
			}
			player->connect_time = atoi(value);
			free(value);
		}
		*last_player = player;
		last_player = &player->next;
		//fprintf( stderr, "Player '%s'\n", player->name );
	}

	return (DONE_FORCE);
}


static const char hl2_statusresponse[] = "\xFF\xFF\xFF\xFF\x49";
static const char hl2_playersresponse[] = "\xFF\xFF\xFF\xFF\x44";
static const char hl2_rulesresponse[] = "\xFF\xFF\xFF\xFF\x45";
static const int hl2_response_size = sizeof(hl2_statusresponse) - 1;
#define HL2_STATUS	1
#define HL2_PLAYERS	2
#define HL2_RULES	3
query_status_t
deal_with_hl2_packet(struct qserver *server, char *rawpkt, int pktlen)
{
	char *ptr = rawpkt;
	char *end = rawpkt + pktlen;
	char temp[512];
	int type = 0;
	unsigned char protocolver = 0;
	int n_sent = 0;

	debug(2, "deal_with_hl2_packet %p, %d", server, pktlen);

	server->n_servers++;
	if (server->server_name == NULL) {
		server->ping_total += time_delta(&packet_recv_time, &server->packet_time1);
	} else {
		gettimeofday(&server->packet_time1, NULL);
	}

	// Check if correct reply
	if (pktlen < hl2_response_size) {
		malformed_packet(server, "short response type");
		return (PKT_ERROR);
	} else {
		if (0 == memcmp(hl2_statusresponse, ptr, hl2_response_size)) {
			if (pktlen < hl2_response_size + 20) {
				malformed_packet(server, "short packet");
				return (PKT_ERROR);
			}
			type = HL2_STATUS;
		} else if (0 == memcmp(hl2_playersresponse, ptr, hl2_response_size)) {
			type = HL2_PLAYERS;
		} else if (0 == memcmp(hl2_rulesresponse, ptr, hl2_response_size)) {
			type = HL2_RULES;
		} else {
			malformed_packet(server, "unknown response");
			return (PKT_ERROR);
		}
	}

	// header
	ptr += hl2_response_size;

	switch (type) {
	case HL2_STATUS:

		// protocol version
		protocolver = *ptr;
		ptr++;

		debug(2, "protocol: 0x%02X", protocolver);
		// Commented out till out of beta

		/*
		 * if( '\x02' != protocolver )
		 * {
		 *  malformed_packet(server, "protocol version != 0x02");
		 *  return PKT_ERROR;
		 * }
		 */

		server->protocol_version = protocolver;
		sprintf(temp, "%d", protocolver);
		add_rule(server, "protocol", temp, NO_FLAGS);

		// server name
		server->server_name = strdup(ptr);
		ptr += strlen(ptr) + 1;

		// map
		server->map_name = strdup(ptr);
		ptr += strlen(ptr) + 1;

		// gamedir
		server->game = strdup(ptr);
		add_rule(server, "gamedir", ptr, NO_FLAGS);
		ptr += strlen(ptr) + 1;

		// description
		add_rule(server, "description", ptr, NO_FLAGS);
		ptr += strlen(ptr) + 1;

		// appid
		ptr += 2;

		// num players
		server->num_players = *ptr;
		ptr++;

		// max players
		server->max_players = *ptr;
		ptr++;

		// bot players
		sprintf(temp, "%hhu", (*ptr));
		add_rule(server, "bot_players", temp, NO_FLAGS);
		ptr++;

		// dedicated
		if ('d' == *ptr) {
			add_rule(server, "sv_type", "dedicated", NO_FLAGS);
		} else if ('l' == *ptr) {
			add_rule(server, "sv_type", "listen", NO_FLAGS);
		} else {
			char tmp[2] =
			{
				*ptr, '\0'
			}


			;
			add_rule(server, "sv_type", tmp, NO_FLAGS);
		}
		ptr++;

		// OS
		if ('l' == *ptr) {
			add_rule(server, "sv_os", "linux", NO_FLAGS);
		} else if ('w' == *ptr) {
			add_rule(server, "sv_os", "windows", NO_FLAGS);
		} else {
			char tmp[2] =
			{
				*ptr, '\0'
			}


			;
			add_rule(server, "sv_os", tmp, NO_FLAGS);
		}
		ptr++;

		// passworded
		add_rule(server, "sv_password", *ptr ? "1" : "0", NO_FLAGS);
		ptr++;

		// secure
		add_rule(server, "secure", *ptr ? "1" : "0", NO_FLAGS);
		ptr++;

		// send the other request packets if wanted
		if (get_server_rules) {
			int requests = server->n_requests;
			debug(3, "send_rule_request_packet7");
			send_rule_request_packet(server);
			server->n_requests = requests;          // prevent wrong ping
			n_sent++;
		} else if (get_player_info) {
			int requests = server->n_requests;
			send_player_request_packet(server);
			server->n_requests = requests;          // prevent wrong ping
			n_sent++;
		}
		break;

	case HL2_RULES:
		// num_players
		ptr++;

		// max_players
		ptr++;
		while (ptr < end) {
			char *var = ptr;
			char *val;
			ptr += strlen(var) + 1;
			val = ptr;
			ptr += strlen(val) + 1;
			add_rule(server, var, val, NO_FLAGS);
		}

		if (get_player_info) {
			send_player_request_packet(server);
			n_sent++;
		}
		break;

	case HL2_PLAYERS:
		// num_players
		ptr++;

		while (ptr < end) {
			struct player *player = add_player(server, server->n_player_info);

			// player no
			ptr++;

			// name
			player->name = strdup(ptr);
			ptr += strlen(ptr) + 1;

			// frags
			player->frags = swap_long_from_little(ptr);
			ptr += 4;

			// time
			player->connect_time = swap_float_from_little(ptr);
			ptr += 4;
		}

		break;

	default:
		malformed_packet(server, "unknown response");
		return (PKT_ERROR);
	}

	return ((0 == n_sent) ? DONE_FORCE : INPROGRESS);
}


query_status_t
deal_with_gamespy_master_response(struct qserver *server, char *rawpkt, int pktlen)
{
	char *data;
	int len;
	int is_binary_packet = 0;

	char *ipptr, *portptr;
	char ipstr[16];
	char portstr[6];
	unsigned int ipaddr;
	unsigned short port;

	server->master_pkt_len = 0;
	server->master_pkt = NULL;

	debug(2, "deal_with_gamespy_master_response %p, %d", server, pktlen);

	if (server->saved_data.data == NULL) {
		debug(2, "gamespy master response first packet");
		server->saved_data.data = (char *)malloc(pktlen);
		if (server->saved_data.data == NULL) {
			debug(0, "Failed to malloc memory for saved data");
			return (MEM_ERROR);
		}
	} else {
		debug(2, "gamespy master response intermediate packet");
		server->saved_data.data = (char *)realloc(server->saved_data.data, server->saved_data.datalen + pktlen);
		if (server->saved_data.data == NULL) {
			debug(0, "Failed to realloc memory for saved data");
			return (MEM_ERROR);
		}
	}

	memcpy(server->saved_data.data + server->saved_data.datalen, rawpkt, pktlen);
	server->saved_data.datalen += pktlen;

	if ((pktlen == 0) || ((pktlen > 7) && (strncmp(rawpkt + pktlen - 7, "\\final\\", 7) == 0))) {
		debug(2, "gamespy master response final packet");

		// the header is in the form \basic\\secure\XXXXXX
		if (strncmp(server->saved_data.data, "\\basic\\\\secure\\", 15) != 0) {
			malformed_packet(server, "gamespy master response unknown header");
			return (PKT_ERROR);
		}

		server->server_name = GAMESPY_MASTER_NAME;

		data = server->saved_data.data + 21;
		len = server->saved_data.datalen - 28;

		if (strncmp(data, "\\ip\\", 4) != 0) {
			is_binary_packet = 1;
		}

		if (is_binary_packet) {
			debug(1, "gamespy master response uses binary packet format");
			server->master_pkt_len = len;
			server->master_pkt = (char *)malloc(len);
			if (server->master_pkt == NULL) {
				debug(0, "Failed to malloc memory for internal master packet");
				return (MEM_ERROR);
			}
			memcpy(server->master_pkt, data, server->master_pkt_len);
			return (DONE_FORCE);
		}

		debug(1, "gamespy master response uses text packet format");

		while (len != 0) {
			// ip entry
			if (strncmp(data, "\\ip\\", 4) == 0) {
				debug(1, "gamespy master response address entry");
				data += 4;
				len -= 4;

				server->master_pkt_len += 6;
				if (server->master_pkt == NULL) {
					server->master_pkt = (char *)malloc(server->master_pkt_len);
					if (server->master_pkt == NULL) {
						debug(0, "Failed to malloc memory for internal master packet");
						return (MEM_ERROR);
					}
				} else {
					server->master_pkt = (char *)realloc(server->master_pkt, server->master_pkt_len);
					if (server->master_pkt == NULL) {
						debug(0, "Failed to realloc memory for internal master packet");
						return (MEM_ERROR);
					}
				}

				ipptr = data;
				portptr = NULL;

				// walk to next entry
				for ( ; len && *data != '\\'; data++, len--) {
					// port found (ip end)
					if (*data == ':') {
						portptr = data + 1;
					}
				}
				if (portptr != NULL) {
					strncpy(ipstr, ipptr, portptr - 1 - ipptr);
					ipstr[portptr - 1 - ipptr] = '\0';

					strncpy(portstr, portptr, data - portptr);
					portstr[data - portptr] = '\0';

					ipaddr = inet_addr(ipstr);
					port = htons((unsigned short)atoi(portstr));
				} else {
					strncpy(ipstr, ipptr, data - 1 - ipptr);
					ipstr[portptr - ipptr] = '\0';

					ipaddr = inet_addr(ipstr);
					port = htons(28000); // default port
				}
				memcpy(server->master_pkt + server->master_pkt_len - 6, &ipaddr, 4);
				memcpy(server->master_pkt + server->master_pkt_len - 2, &port, 2);
			} else {
				malformed_packet(server, "gamespy master response unknown entry");
				return (PKT_ERROR);
			}
		}

		server->n_servers = server->master_pkt_len / 6;
		server->next_player_info = -1;
		server->retry1 = 0;

		return (DONE_FORCE);
	}

	return (INPROGRESS);
}


/* Misc utility functions
 */
unsigned int
swap_long(void *l)
{
	unsigned char *b = (unsigned char *)l;

	return (b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24));
}


unsigned short
swap_short(void *l)
{
	unsigned char *b = (unsigned char *)l;

	return ((unsigned short)b[0] | (b[1] << 8));
}


unsigned int
swap_long_from_little(void *l)
{
	unsigned char *b = (unsigned char *)l;
	unsigned int result;

	if (little_endian) {
		memcpy(&result, l, 4);
	} else {
		result = (unsigned int)b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
	}
	return (result);
}


float
swap_float_from_little(void *f)
{
	union {
		int i;
		float fl;
	}
	temp;

	temp.i = swap_long_from_little(f);
	return (temp.fl);
}


unsigned short
swap_short_from_little(void *l)
{
	unsigned char *b = (unsigned char *)l;
	unsigned short result;

	if (little_endian) {
		memcpy(&result, l, 2);
	} else {
		result = (unsigned short)b[0] | ((unsigned short)b[1] << 8);
	}
	return (result);
}


/** write four byte to buf */
void
put_long_little(unsigned val, char *buf)
{
	buf[0] = val & 0xFF;
	buf[1] = (val >> 8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
}


char *
xml_escape(char *string)
{
	static unsigned char _buf[4][MAXSTRLEN + 8];
	static int _buf_index = 0;
	unsigned char *result, *b, *end;
	unsigned int c;

	if (string == NULL) {
		return ("");
	}

	result = &_buf[_buf_index][0];
	_buf_index = (_buf_index + 1) % 4;

	end = &result[MAXSTRLEN];

	b = result;
	for ( ; *string && b < end; string++) {
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
			    (0x09 == c) ||
			    (0xA == c) ||
			    (0xD == c) ||
			    ((0x20 <= c) && (0xD7FF >= c)) ||
			    ((0xE000 <= c) && (0xFFFD >= c)) ||
			    ((0x10000 <= c) && (0x10FFFF >= c))
		    )
		    ) {
			if (show_errors) {
				fprintf(stderr, "Encoding error (%d) for U+%x, D+%d\n", 1, c, c);
			}
		} else if (xml_encoding == ENCODING_LATIN_1) {
			if (!xform_names) {
				*b++ = c;
			} else {
				if (isprint(c)) {
					*b++ = c;
				} else {
					b += sprintf((char *)b, "&#%u;", c);
				}
			}
		} else if (xml_encoding == ENCODING_UTF_8) {
			unsigned char tempbuf[10] =
			{
				0
			};
			unsigned char *buf = &tempbuf[0];
			int bytes = 0;
			int error = 1;

			// Valid character ranges
			if (
				(0x09 == c) ||
				(0xA == c) ||
				(0xD == c) ||
				((0x20 <= c) && (0xD7FF >= c)) ||
				((0xE000 <= c) && (0xFFFD >= c)) ||
				((0x10000 <= c) && (0x10FFFF >= c))
				) {
				error = 0;
			}

			if (c < 0x80) {
				/* 0XXX XXXX one byte */
				buf[0] = c;
				bytes = 1;
			} else if (c < 0x0800) {
				/* 110X XXXX two bytes */
				buf[0] = 0xC0 | (0x03 & (c >> 6));
				buf[1] = 0x80 | (0x3F & c);
				bytes = 2;
			} else if (c < 0x10000) {
				/* 1110 XXXX three bytes */
				buf[0] = 0xE0 | (c >> 12);
				buf[1] = 0x80 | ((c >> 6) & 0x3F);
				buf[2] = 0x80 | (c & 0x3F);

				bytes = 3;
				if ((c == UTF8BYTESWAPNOTACHAR) || (c == UTF8NOTACHAR)) {
					error = 3;
				}
			} else if (c < 0x10FFFF) {
				/* 1111 0XXX four bytes */
				buf[0] = 0xF0 | (c >> 18);
				buf[1] = 0x80 | ((c >> 12) & 0x3F);
				buf[2] = 0x80 | ((c >> 6) & 0x3F);
				buf[3] = 0x80 | (c & 0x3F);
				bytes = 4;
				if (c > UTF8MAXFROMUCS4) {
					error = 4;
				}
			} else if (c < 0x4000000) {
				/* 1111 10XX five bytes */
				buf[0] = 0xF8 | (c >> 24);
				buf[1] = 0x80 | (c >> 18);
				buf[2] = 0x80 | ((c >> 12) & 0x3F);
				buf[3] = 0x80 | ((c >> 6) & 0x3F);
				buf[4] = 0x80 | (c & 0x3F);
				bytes = 5;
				error = 5;
			} else if (c < 0x80000000) {
				/* 1111 110X six bytes */
				buf[0] = 0xFC | (c >> 30);
				buf[1] = 0x80 | ((c >> 24) & 0x3F);
				buf[2] = 0x80 | ((c >> 18) & 0x3F);
				buf[3] = 0x80 | ((c >> 12) & 0x3F);
				buf[4] = 0x80 | ((c >> 6) & 0x3F);
				buf[5] = 0x80 | (c & 0x3F);
				bytes = 6;
				error = 6;
			} else {
				error = 7;
			}

			if (error) {
				int i;
				fprintf(stderr, "UTF-8 encoding error (%d) for U+%x, D+%d : ", error, c, c);
				for (i = 0; i < bytes; i++) {
					fprintf(stderr, "0x%02x ", buf[i]);
				}
				fprintf(stderr, "\n");
			} else {
				int i;
				for (i = 0; i < bytes; ++i) {
					*b++ = buf[i];
				}
			}
		}
	}
	*b = '\0';
	return ((char *)result);
}


int
is_default_rule(struct rule *rule)
{
	if (strcmp(rule->name, "sv_maxspeed") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_SV_MAXSPEED) == 0);
	}
	if (strcmp(rule->name, "sv_friction") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_SV_FRICTION) == 0);
	}
	if (strcmp(rule->name, "sv_gravity") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_SV_GRAVITY) == 0);
	}
	if (strcmp(rule->name, "noexit") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_NOEXIT) == 0);
	}
	if (strcmp(rule->name, "teamplay") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_TEAMPLAY) == 0);
	}
	if (strcmp(rule->name, "timelimit") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_TIMELIMIT) == 0);
	}
	if (strcmp(rule->name, "fraglimit") == 0) {
		return (strcmp(rule->value, Q_DEFAULT_FRAGLIMIT) == 0);
	}
	return (0);
}


char *
strherror(int h_err)
{
	static char msg[100];

	switch (h_err) {
	case HOST_NOT_FOUND:
		return ("host not found");

	case TRY_AGAIN:
		return ("try again");

	case NO_RECOVERY:
		return ("no recovery");

	case NO_ADDRESS:
		return ("no address");

	default:
		sprintf(msg, "%d", h_err);
		return (msg);
	}
}


int
time_delta(struct timeval *later, struct timeval *past)
{
	if (later->tv_usec < past->tv_usec) {
		later->tv_sec--;
		later->tv_usec += 1000000;
	}
	return ((later->tv_sec - past->tv_sec) * 1000 + (later->tv_usec - past->tv_usec) / 1000);
}


int
connection_inprogress()
{
#ifdef _WIN32
		return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
		return (errno == EINPROGRESS);
#endif
}


int
connection_refused()
{
#ifdef _WIN32
		return (WSAGetLastError() == WSAECONNABORTED);
#else
		return (errno == ECONNREFUSED);
#endif
}


int
connection_would_block()
{
#ifdef _WIN32
		return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
		return (errno == EAGAIN);
#endif
}


int
connection_reset()
{
#ifdef _WIN32
		return (WSAGetLastError() == WSAECONNRESET);
#else
		return (errno == ECONNRESET);
#endif
}


void
clear_socketerror()
{
#ifdef _WIN32
		WSASetLastError(0);
#else
		errno = 0;
#endif
}


void
set_non_blocking(int fd)
{
#ifdef _WIN32
		int one = 1;
		ioctlsocket(fd, FIONBIO, (unsigned long *)&one);
#else
 #ifdef O_NONBLOCK
			fcntl(fd, F_SETFL, O_NONBLOCK);
 #else
			fcntl(fd, F_SETFL, O_NDELAY);
 #endif         // O_NONBLOCK
#endif  // _WIN32
}


char *
quake_color(int color)
{
	static char *colors[] =
	{
		"White",        /* 0 */
		"Brown",        /* 1 */
		"Lavender",     /* 2 */
		"Khaki",        /* 3 */
		"Red",          /* 4 */
		"Lt Brown",     /* 5 */
		"Peach",        /* 6 */
		"Lt Peach",     /* 7 */
		"Purple",       /* 8 */
		"Dk Purple",    /* 9 */
		"Tan",          /* 10 */
		"Green",        /* 11 */
		"Yellow",       /* 12 */
		"Blue",         /* 13 */
		"Blue",         /* 14 */
		"Blue"          /* 15 */
	};

	static char *rgb_colors[] =
	{
		"#ffffff",      /* 0 */
		"#8b4513",      /* 1 */
		"#e6e6fa",      /* 2 */
		"#f0e68c",      /* 3 */
		"#ff0000",      /* 4 */
		"#deb887",      /* 5 */
		"#eecbad",      /* 6 */
		"#ffdab9",      /* 7 */
		"#9370db",      /* 8 */
		"#5d478b",      /* 9 */
		"#d2b48c",      /* 10 */
		"#00ff00",      /* 11 */
		"#ffff00",      /* 12 */
		"#0000ff",      /* 13 */
		"#0000ff",      /* 14 */
		"#0000ff"       /* 15 */
	};

	static char *color_nr[] =
	{
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
	};

	if (color_names) {
		if (color_names == 1) {
			return (colors[color & 0xf]);
		} else {
			return (rgb_colors[color & 0xf]);
		}
	} else {
		return (color_nr[color & 0xf]);
	}
}


#define FMT_HOUR_1	"%2dh"
#define FMT_HOUR_2	"%dh"
#define FMT_MINUTE_1	"%2dm"
#define FMT_MINUTE_2	"%dm"
#define FMT_SECOND_1	"%2ds"
#define FMT_SECOND_2	"%ds"

char *
play_time(int seconds, int show_seconds)
{
	static char time_string[24];

	if (time_format == CLOCK_TIME) {
		char *fmt_hour = show_seconds == 2 ? FMT_HOUR_2 : FMT_HOUR_1;
		char *fmt_minute = show_seconds == 2 ? FMT_MINUTE_2 : FMT_MINUTE_1;
		char *fmt_second = show_seconds == 2 ? FMT_SECOND_2 : FMT_SECOND_1;
		time_string[0] = '\0';
		if (seconds / 3600) {
			sprintf(time_string, fmt_hour, seconds / 3600);
		} else if (show_seconds < 2) {
			strcat(time_string, "   ");
		}
		if ((seconds % 3600) / 60 || seconds / 3600) {
			sprintf(time_string + strlen(time_string), fmt_minute, (seconds % 3600) / 60);
		} else if (!show_seconds) {
			sprintf(time_string + strlen(time_string), " 0m");
		} else if (show_seconds < 2) {
			strcat(time_string, "   ");
		}
		if (show_seconds) {
			sprintf(time_string + strlen(time_string), fmt_second, seconds % 60);
		}
	} else if (time_format == STOPWATCH_TIME) {
		if (show_seconds) {
			sprintf(time_string, "%02d:%02d:%02d", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
		} else {
			sprintf(time_string, "%02d:%02d", seconds / 3600, (seconds % 3600) / 60);
		}
	} else {
		sprintf(time_string, "%d", seconds);
	}

	return (time_string);
}


char *
ping_time(int ms)
{
	static char time_string[24];

	if (ms < 1000) {
		sprintf(time_string, "%dms", ms);
	} else if (ms < 1000000) {
		sprintf(time_string, "%ds", ms / 1000);
	} else {
		sprintf(time_string, "%dm", ms / 1000 / 60);
	}
	return (time_string);
}


int
count_bits(int n)
{
	int b = 0;

	for ( ; n; n >>= 1) {
		if (n & 1) {
			b++;
		}
	}
	return (b);
}


int
strcmp_withnull(char *one, char *two)
{
	if ((one == NULL) && (two == NULL)) {
		return (0);
	}
	if ((one != NULL) && (two == NULL)) {
		return (-1);
	}
	if (one == NULL) {
		return (1);
	}
	return (strcasecmp(one, two));
}


/*
 * Sorting functions
 */
void
sort_servers(struct qserver **array, int size)
{
	quicksort((void **)array, 0, size - 1, (int (*)(void *, void *))server_compare);
}


void
sort_players(struct qserver *server)
{
	struct player **array, *player, *last_team = NULL, **next;
	int np, i;

	if ((server->num_players == 0) || (server->players == NULL)) {
		return;
	}

	player = server->players;
	for ( ; player != NULL && player->number == TRIBES_TEAM; ) {
		last_team = player;
		player = player->next;
	}

	if (player == NULL) {
		return;
	}

	array = (struct player **)malloc(sizeof(struct player *) * (server->num_players + server->num_spectators));
	for (np = 0; player != NULL && np < server->num_players + server->num_spectators; np++) {
		array[np] = player;
		player = player->next;
	}
	quicksort((void **)array, 0, np - 1, (int (*)(void *, void *))player_compare);

	if (last_team) {
		next = &last_team->next;
	} else {
		next = &server->players;
	}

	for (i = 0; i < np; i++) {
		*next = array[i];
		array[i]->next = NULL;
		next = &array[i]->next;
	}

	free(array);
}


int
server_compare(struct qserver *one, struct qserver *two)
{
	int rc;

	char *key = sort_keys;

	for ( ; *key; key++) {
		switch (*key) {
		case 'g':
			rc = strcmp_withnull(one->game, two->game);
			if (rc) {
				return (rc);
			}
			break;

		case 'p':
			if (one->n_requests == 0) {
				return (two->n_requests);
			} else if (two->n_requests == 0) {
				return (-1);
			}
			rc = one->ping_total / one->n_requests - two->ping_total / two->n_requests;
			if (rc) {
				return (rc);
			}
			break;

		case 'i':
			if (one->ipaddr > two->ipaddr) {
				return (1);
			} else if (one->ipaddr < two->ipaddr) {
				return (-1);
			} else if (one->port > two->port) {
				return (1);
			} else if (one->port < two->port) {
				return (-1);
			}
			break;

		case 'h':
			rc = strcmp_withnull(one->host_name, two->host_name);
			if (rc) {
				return (rc);
			}
			break;

		case 'n':
			rc = two->num_players - one->num_players;
			if (rc) {
				return (rc);
			}
			break;
		}
	}

	return (0);
}


int
type_option_compare(server_type *one, server_type *two)
{
	return (strcmp_withnull(one->type_option, two->type_option));
}


int
type_string_compare(server_type *one, server_type *two)
{
	return (strcmp_withnull(one->type_string, two->type_string));
}


int
player_compare(struct player *one, struct player *two)
{
	int rc;

	char *key = sort_keys;

	for ( ; *key; key++) {
		switch (*key) {
		case 'P':
			rc = one->ping - two->ping;
			if (rc) {
				return (rc);
			}
			break;

		case 'F':
			rc = two->frags - one->frags;
			if (rc) {
				return (rc);
			}
			break;

		case 'S':
			rc = two->score - one->score;
			if (rc) {
				return (rc);
			}
			break;

		case 'T':
			rc = one->team - two->team;
			if (rc) {
				return (rc);
			}
			rc = strcmp_withnull(one->team_name, two->team_name);
			if (rc) {
				return (rc);
			}
			break;

		case 'N':
			rc = strcmp_withnull(one->name, two->name);
			if (rc) {
				return (rc);
			}
			return (one->number - two->number);

			break;
		}
	}
	return (0);
}


void
quicksort(void **array, int i, int j, int (*compare)(void *, void *))
{
	int q = 0;

	if (i < j) {
		q = qpartition(array, i, j, compare);
		quicksort(array, i, q, compare);
		quicksort(array, q + 1, j, compare);
	}
}


int
qpartition(void **array, int a, int b, int (*compare)(void *, void *))
{
	/* this is our comparison point. when we are done
	 * splitting this array into 2 parts, we want all the
	 * elements on the left side to be less then or equal
	 * to this, all the elements on the right side need to
	 * be greater then or equal to this
	 */
	void *z;

	/* indicies into the array to sort. Used to calculate a partition
	 * point
	 */
	int i = a - 1;
	int j = b + 1;

	/* temp pointer used to swap two array elements */
	void *tmp = NULL;

	z = array[a];

	while (1) {
		/* move the right indice over until the value of that array
		 * elem is less than or equal to z. Stop if we hit the left
		 * side of the array (ie, j == a);
		 */
		do {
			j--;
		} while (j > a && compare(array[j], z) > 0);

		/* move the left indice over until the value of that
		 * array elem is greater than or equal to z, or until
		 * we hit the right side of the array (ie i == j)
		 */
		do {
			i++;
		} while (i <= j && compare(array[i], z) < 0);

		/* if i is less then j, we need to switch those two array
		 * elements, if not then we are done partitioning this array
		 * section
		 */
		if (i < j) {
			tmp = array[i];
			array[i] = array[j];
			array[j] = tmp;
		} else {
			return (j);
		}
	}
}
