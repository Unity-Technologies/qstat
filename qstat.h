/*
 * qstat.h
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Copyright 1996,1997,1998,1999,2000,2001,2002 by Steve Jankowski
 */

#ifndef __H_QSTAT
#define __H_QSTAT

#ifdef HAVE_CONFIG_H
#include "gnuconfig.h"
#endif

#ifdef __EMX__
#include <sys/select.h>
#include <sys/param.h>
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#ifdef _WIN32
# include <sys/timeb.h>
# include <winsock.h>
# define PATH_MAX MAX_PATH
# include <fcntl.h>
# define _POSIX_ 1
# ifndef FD_SETSIZE
#  define FD_SETSIZE 256
# endif
# define close(a) closesocket(a)
static int gettimeofday(struct timeval *now, void *blah)
{
    struct timeb timeb;
    ftime( &timeb);
    now->tv_sec= timeb.time;
    now->tv_usec= (unsigned int)timeb.millitm * 1000;
    return 0;
}
# define sockerr()	WSAGetLastError()
# define strcasecmp      stricmp
# define strncasecmp     strnicmp
# define STATIC
# ifndef EADDRINUSE
#  define EADDRINUSE	WSAEADDRINUSE
# endif
# define snprintf _snprintf
#else
# include <sys/time.h>
# define SOCKET_ERROR -1
#endif /* _WIN32 */

#include <string.h>

typedef struct _server_type server_type;

#ifdef __GNUC__
#define GCC_FORMAT_PRINTF(a, b) __attribute__ ((format (printf, a, b)))
#else
#define GCC_FORMAT_PRINTF(a, b)
#endif

#include "qserver.h"

// Packet modules
#include "ut2004.h"
#include "doom3.h"
#include "a2s.h"
#include "fl.h"
#include "gps.h"
#include "gs2.h"
#include "gs3.h"
#include "haze.h"
#include "ts2.h"
#include "tm.h"
#include "wic.h"
#include "ottd.h"

/*
 * Various magic numbers.
 */

#define Q_DEFAULT_PORT			26000
#define HEXEN2_DEFAULT_PORT		26900
#define Q2_DEFAULT_PORT			27910
#define Q3_DEFAULT_PORT			27960
#define Q2_MASTER_DEFAULT_PORT		27900
#define Q3_MASTER_DEFAULT_PORT		27950
#define QW_DEFAULT_PORT			27500
#define QW_MASTER_DEFAULT_PORT		27000
#define HW_DEFAULT_PORT			26950
#define HW_MASTER_DEFAULT_PORT		26900
#define UNREAL_DEFAULT_PORT		7777
#define UNREAL_MASTER_DEFAULT_PORT	28900
#define HALFLIFE_DEFAULT_PORT		27015
#define HL_MASTER_DEFAULT_PORT		27010
#define SIN_DEFAULT_PORT		22450
#define SHOGO_DEFAULT_PORT		27888
#define TRIBES_DEFAULT_PORT		28001
#define TRIBES_MASTER_DEFAULT_PORT	28000
#define BFRIS_DEFAULT_PORT		44001
#define KINGPIN_DEFAULT_PORT		31510
#define HERETIC2_DEFAULT_PORT		28910
#define SOF_DEFAULT_PORT		28910
#define GAMESPY_MASTER_DEFAULT_PORT	28900
#define TRIBES2_DEFAULT_PORT		28000
#define TRIBES2_MASTER_DEFAULT_PORT	28002
#define DESCENT3_GAMESPY_DEFAULT_PORT	20142
#define DESCENT3_DEFAULT_PORT		2092
#define DESCENT3_MASTER_DEFAULT_PORT	3445
#define RTCW_DEFAULT_PORT		27960
#define RTCW_MASTER_DEFAULT_PORT	27950
#define STEF_DEFAULT_PORT		27960
#define STEF_MASTER_DEFAULT_PORT	27953
#define JK3_DEFAULT_PORT		29070
#define JK3_MASTER_DEFAULT_PORT	29060
#define GHOSTRECON_PLAYER_DEFAULT_PORT	2346
#define RAVENSHIELD_DEFAULT_PORT	8777
#define SAVAGE_DEFAULT_PORT	11235
#define FARCRY_DEFAULT_PORT	49001
#define STEAM_MASTER_DEFAULT_PORT	27010
#define HL2_DEFAULT_PORT	27015
#define HL2_MASTER_DEFAULT_PORT	27011
#define TS2_DEFAULT_PORT 51234
#define TM_DEFAULT_PORT 5000
#define WIC_DEFAULT_PORT 5000 // Default is actually disabled
#define FL_DEFAULT_PORT 5478


#define Q_UNKNOWN_TYPE 0
#define MASTER_SERVER 0x40000000

#define Q_SERVER	1
#define QW_SERVER	2
#define QW_MASTER	(3 | MASTER_SERVER)
#define H2_SERVER	4
#define Q2_SERVER	5
#define Q2_MASTER	(6|MASTER_SERVER)
#define HW_SERVER	7
#define UN_SERVER	8
#define UN_MASTER	(9|MASTER_SERVER)
#define HL_SERVER	10
#define HL_MASTER	(11|MASTER_SERVER)
#define SIN_SERVER	12
#define SHOGO_SERVER	13
#define TRIBES_SERVER	14
#define TRIBES_MASTER	(15|MASTER_SERVER)
#define Q3_SERVER	16
#define Q3_MASTER	(17|MASTER_SERVER)
#define BFRIS_SERVER	18
#define KINGPIN_SERVER	19
#define HERETIC2_SERVER	20
#define SOF_SERVER	21
#define GAMESPY_PROTOCOL_SERVER	22
#define GAMESPY_MASTER	(23|MASTER_SERVER)
#define TRIBES2_SERVER	24
#define TRIBES2_MASTER	(25|MASTER_SERVER)
#define DESCENT3_GAMESPY_SERVER	26
#define DESCENT3_PXO_SERVER	27
#define DESCENT3_SERVER	28
#define DESCENT3_MASTER	(29|MASTER_SERVER)
#define RTCW_SERVER	30
#define RTCW_MASTER	(31|MASTER_SERVER)
#define STEF_SERVER	32
#define STEF_MASTER	(33|MASTER_SERVER)
#define UT2003_SERVER	34
#define GHOSTRECON_SERVER 35
#define ALLSEEINGEYE_PROTOCOL_SERVER 36
#define RAVENSHIELD_SERVER 37
#define SAVAGE_SERVER 38
#define FARCRY_SERVER 39
#define GAMESPY2_PROTOCOL_SERVER 40
#define STEAM_MASTER (41|MASTER_SERVER)
#define JK3_SERVER 42
#define JK3_MASTER (43|MASTER_SERVER)
#define DOOM3_SERVER 44
#define DOOM3_MASTER (45|MASTER_SERVER)
#define HL2_SERVER 46
#define HL2_MASTER (47|MASTER_SERVER)
#define UT2004_MASTER (48|MASTER_SERVER)
#define A2S_SERVER	49
#define PARIAH_SERVER	50
#define GAMESPY3_PROTOCOL_SERVER 51
#define TS2_PROTOCOL_SERVER 52
#define QUAKE4_SERVER 53
#define QUAKE4_MASTER (53|MASTER_SERVER)
#define ARMYOPS_SERVER 54
#define GAMESPY4_PROTOCOL_SERVER 55
#define PREY_SERVER 56
#define TM_PROTOCOL_SERVER 57
#define ETQW_SERVER 58
#define HAZE_SERVER 59
#define HW_MASTER (60 | MASTER_SERVER)
#define WIC_PROTOCOL_SERVER 61
#define OTTD_SERVER 62
#define OTTD_MASTER (63 | MASTER_SERVER)
#define FL_SERVER	64

#define LAST_BUILTIN_SERVER  64

#define TF_SINGLE_QUERY		(1<<1)
#define TF_OUTFILE		(1<<2)
#define TF_MASTER_MULTI_RESPONSE	(1<<3)
#define TF_TCP_CONNECT		(1<<4)
#define TF_QUERY_ARG		(1<<5)
#define TF_QUERY_ARG_REQUIRED	(1<<6)
#define TF_QUAKE3_NAMES		(1<<7)
#define TF_TRIBES2_NAMES	(1<<8)
#define TF_SOF_NAMES		(1<<9)
#define TF_U2_NAMES		(1<<10)

#define TF_RAW_STYLE_QUAKE	(1<<11)
#define TF_RAW_STYLE_TRIBES	(1<<12)
#define TF_RAW_STYLE_GHOSTRECON	(1<<13)

#define TF_NO_PORT_OFFSET	(1<<14)
#define TF_SHOW_GAME_PORT	(1<<15)

#define TF_MASTER_STEAM		(1<<16)  /* supports steam server filter */

// What response type are we expecting
// XXX: this is not what server->flags is for
#define TF_STATUS_QUERY		(1<<17)
#define TF_PLAYER_QUERY		(1<<18)
#define TF_RULES_QUERY		(1<<19)
#define TF_TM_NAMES			(1<<20)

#define TRIBES_TEAM	-1

struct q_packet;

typedef enum {
	INPROGRESS =  0,
	DONE_AUTO  =  1,
	DONE_FORCE =  2,

	SYS_ERROR  = -1,
	MEM_ERROR  = -2,
	PKT_ERROR  = -3,
	ORD_ERROR  = -4
} query_status_t;

typedef void (*DisplayFunc)( struct qserver *);
typedef query_status_t (*QueryFunc)( struct qserver *);
typedef int (*PacketFunc)( struct qserver *, char *rawpkt, int pktlen);

/*
 * Output and formatting functions
 */

void display_server( struct qserver *server);
void display_qwmaster( struct qserver *server);
void display_server_rules( struct qserver *server);
void display_player_info( struct qserver *server);
void display_q_player_info( struct qserver *server);
void display_qw_player_info( struct qserver *server);
void display_q2_player_info( struct qserver *server);
void display_unreal_player_info( struct qserver *server);
void display_shogo_player_info( struct qserver *server);
void display_halflife_player_info( struct qserver *server);
void display_tribes_player_info( struct qserver *server);
void display_tribes2_player_info( struct qserver *server);
void display_bfris_player_info( struct qserver *server);
void display_descent3_player_info( struct qserver *server);
void display_ravenshield_player_info( struct qserver *server);
void display_savage_player_info( struct qserver *server);
void display_farcry_player_info( struct qserver *server);
void display_ghostrecon_player_info( struct qserver *server);
void display_eye_player_info( struct qserver *server);
void display_armyops_player_info( struct qserver *server);
void display_gs2_player_info( struct qserver *server);
void display_doom3_player_info( struct qserver *server);
void display_hl2_player_info( struct qserver *server);
void display_ts2_player_info( struct qserver *server);
void display_tm_player_info( struct qserver *server);
void display_haze_player_info( struct qserver *server);
void display_wic_player_info( struct qserver *server);
void display_fl_player_info( struct qserver *server);

void raw_display_server( struct qserver *server);
void raw_display_server_rules( struct qserver *server);
void raw_display_player_info( struct qserver *server);
void raw_display_q_player_info( struct qserver *server);
void raw_display_qw_player_info( struct qserver *server);
void raw_display_q2_player_info( struct qserver *server);
void raw_display_unreal_player_info( struct qserver *server);
void raw_display_halflife_player_info( struct qserver *server);
void raw_display_tribes_player_info( struct qserver *server);
void raw_display_tribes2_player_info( struct qserver *server);
void raw_display_bfris_player_info( struct qserver *server);
void raw_display_ravenshield_player_info( struct qserver *server);
void raw_display_savage_player_info( struct qserver *server);
void raw_display_farcry_player_info( struct qserver *server);
void raw_display_descent3_player_info( struct qserver *server);
void raw_display_ghostrecon_player_info( struct qserver *server);
void raw_display_eye_player_info( struct qserver *server);
void raw_display_armyops_player_info( struct qserver *server);
void raw_display_gs2_player_info( struct qserver *server);
void raw_display_doom3_player_info( struct qserver *server);
void raw_display_hl2_player_info( struct qserver *server);
void raw_display_ts2_player_info( struct qserver *server);
void raw_display_tm_player_info( struct qserver *server);
void raw_display_haze_player_info( struct qserver *server);
void raw_display_wic_player_info( struct qserver *server);
void raw_display_fl_player_info( struct qserver *server);

void xml_display_server( struct qserver *server);
void xml_header();
void xml_footer();
void xml_display_server_rules( struct qserver *server);
void xml_display_player_info( struct qserver *server);
void xml_display_q_player_info( struct qserver *server);
void xml_display_qw_player_info( struct qserver *server);
void xml_display_q2_player_info( struct qserver *server);
void xml_display_unreal_player_info( struct qserver *server);
void xml_display_halflife_player_info( struct qserver *server);
void xml_display_tribes_player_info( struct qserver *server);
void xml_display_tribes2_player_info( struct qserver *server);
void xml_display_ravenshield_player_info( struct qserver *server);
void xml_display_savage_player_info( struct qserver *server);
void xml_display_farcry_player_info( struct qserver *server);
void xml_display_bfris_player_info( struct qserver *server);
void xml_display_descent3_player_info( struct qserver *server);
void xml_display_ghostrecon_player_info( struct qserver *server);
void xml_display_eye_player_info( struct qserver *server);
void xml_display_armyops_player_info( struct qserver *server);
void xml_display_gs2_player_info( struct qserver *server);
void xml_display_doom3_player_info( struct qserver *server);
void xml_display_hl2_player_info( struct qserver *server);
void xml_display_ts2_player_info( struct qserver *server);
void xml_display_tm_player_info( struct qserver *server);
void xml_display_haze_player_info( struct qserver *server);
void xml_display_wic_player_info( struct qserver *server);
void xml_display_fl_player_info( struct qserver *server);
char *xml_escape( char*);
char *str_replace( char *, char *, char *);

int send_server_request_packet( struct qserver *server);
int send_qserver_request_packet( struct qserver *server);
int send_qwserver_request_packet( struct qserver *server);
int send_ut2003_request_packet( struct qserver *server);
int send_tribes_request_packet( struct qserver *server);
int send_qwmaster_request_packet( struct qserver *server);
int send_bfris_request_packet( struct qserver *server);
int send_player_request_packet( struct qserver *server);
int send_rule_request_packet( struct qserver *server);
int send_ravenshield_request_packet( struct qserver *server);
int send_savage_request_packet( struct qserver *server);
int send_farcry_request_packet( struct qserver *server);
int send_gamespy_master_request( struct qserver *server);
int send_tribes2_request_packet( struct qserver *server);
int send_tribes2master_request_packet( struct qserver *server);
int send_ghostrecon_request_packet( struct qserver *server);
int send_eye_request_packet( struct qserver *server);
int send_gs2_request_packet( struct qserver *server);
int send_doom3_request_packet( struct qserver *server);
int send_hl2_request_packet( struct qserver *server);
int send_ts2_request_packet( struct qserver *server);
int send_tm_request_packet( struct qserver *server);
int send_wic_request_packet( struct qserver *server);

int deal_with_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_q_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_qw_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_q1qw_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_q2_packet( struct qserver *server, char *pkt, int pktlen );
int deal_with_doom3master_packet( struct qserver *server, char *rawpkt, int pktlen);
int deal_with_qwmaster_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_halflife_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_ut2003_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_tribes_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_tribesmaster_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_bfris_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_gamespy_master_response( struct qserver *server, char *pkt, int pktlen);
int deal_with_ravenshield_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_savage_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_farcry_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_tribes2_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_tribes2master_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_descent3_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_descent3master_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_ghostrecon_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_eye_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_doom3_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_hl2_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_ts2_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_tm_packet( struct qserver *server, char *pkt, int pktlen);
int deal_with_wic_packet( struct qserver *server, char *pkt, int pktlen);

struct _server_type  {
    int id;
    char *type_prefix;
    char *type_string;
    char *type_option;
    char *game_name;
    int master;
    unsigned short default_port;
    unsigned short port_offset;
    int flags;
    char *game_rule;
    char *template_var;
    char *status_packet;
    int status_len;
    char *player_packet;
    int player_len;
    char *rule_packet;
    int rule_len;
    char *master_packet;
    int master_len;
    char *master_protocol;
    char *master_query;
    DisplayFunc display_player_func;
    DisplayFunc display_rule_func;
    DisplayFunc display_raw_player_func;
    DisplayFunc display_raw_rule_func;
    DisplayFunc display_xml_player_func;
    DisplayFunc display_xml_rule_func;
    QueryFunc status_query_func;
    QueryFunc rule_query_func;
    QueryFunc player_query_func;
    PacketFunc packet_func;
};

extern server_type builtin_types[];
extern server_type *types;
extern int n_server_types;
extern server_type* default_server_type;
server_type* find_server_type_string( char* type_string);


#ifdef QUERY_PACKETS
#undef QUERY_PACKETS

/* QUAKE */
struct q_packet  {
    unsigned char flag1;
    unsigned char flag2;
    unsigned short length;
    unsigned char op_code;
    unsigned char data[19];
};
#define Q_HEADER_LEN	5

/*
struct {
    unsigned char flag1;
    unsigned char flag2;
    unsigned short length;
    unsigned char op_code;
    char name[6];
    unsigned char version;
};
*/

#define Q_FLAG1			0x80
#define Q_FLAG2			0x00
#define Q_CCREQ_SERVER_INFO	0x02
#define Q_CCREQ_PLAYER_INFO	0x03
#define Q_CCREQ_RULE_INFO	0x04

/* The \003 below is the protocol version */
#define Q_SERVERINFO_LEN	12
struct q_packet q_serverinfo =
{ Q_FLAG1, Q_FLAG2, Q_SERVERINFO_LEN, Q_CCREQ_SERVER_INFO, "QUAKE\000\003" };

struct q_packet q_rule = {Q_FLAG1,Q_FLAG2, 0, Q_CCREQ_RULE_INFO, ""};
struct q_packet q_player = {Q_FLAG1,Q_FLAG2, 6, Q_CCREQ_PLAYER_INFO, ""};

/* QUAKE WORLD */
struct {
    char prefix[4];
    char command[10];
} qw_serverstatus =
{ { '\377', '\377', '\377', '\377' }, { 's', 't', 'a', 't', 'u', 's', ' ', '2', '3', '\n' } };

/* QUAKE3 */
struct {
    char prefix[4];
    char command[10];
} q3_serverstatus =
{ { '\377', '\377', '\377', '\377' }, { 'g', 'e', 't', 's', 't', 'a', 't', 'u', 's', '\n' } };

struct {
    char prefix[4];
    char command[8];
} q3_serverinfo =
{ { '\377', '\377', '\377', '\377' }, { 'g', 'e', 't', 'i', 'n', 'f', 'o', '\n' } };

/* DOOM 3 */
struct {
    char prefix[2];
    char command[12];
} doom3_serverinfo =
{ { '\377', '\377' }, { 'g', 'e', 't', 'I', 'n', 'f', 'o', '\0', '\0', '\0', '\0', '\0' } };

/* HALF-LIFE 2 */
char hl2_serverinfo[20] = { '\xFF', '\xFF', '\xFF', '\xFF', 'T', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
char hl2_playerinfo[20] = { '\xFF', '\xFF', '\xFF', '\xFF', 'U', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
char hl2_ruleinfo[20] = { '\xFF', '\xFF', '\xFF', '\xFF', 'V', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };

/* HEXEN WORLD */
struct {
    char prefix[5];
    char command[7];
} hw_serverstatus =
{ { '\377', '\377', '\377', '\377', '\377' }, { 's', 't', 'a', 't', 'u', 's', '\n' } };

/* HEXEN 2 */
/* The \004 below is the protocol version */
#define H2_SERVERINFO_LEN	14
struct q_packet h2_serverinfo =
{ Q_FLAG1, Q_FLAG2, H2_SERVERINFO_LEN, Q_CCREQ_SERVER_INFO, "HEXENII\000\004" };

/* UNREAL */

char unreal_serverstatus[8] = { '\\', 's','t','a','t','u','s', '\\' };

/*
char unreal_serverstatus[] = { '\\', 's','t','a','t','u','s', '\\', '\\', 'p','l','a','y','e','r','s', '\\', '\\' };
*/
char unreal_masterlist[23] = "\\list\\\\gamename\\unreal";

/* UT 2003 */
char ut2003_basicstatus[] = { 0x78, 0,0,0, 0 };
//char ut2003_ruleinfo[] = { 0x78, 0,0,0, 1 };
//char ut2003_playerinfo[] = { 0x78, 0,0,0, 2 };
char ut2003_allinfo[] = { 0x78, 0,0,0, 3 };

/* Pariah */
char pariah_basicstatus[] = { 0x77, 0,0,0, 0x13 };

/* HALF LIFE */
char hl_ping[9] =
{ '\377', '\377', '\377', '\377', 'p', 'i', 'n', 'g', '\0' };
char hl_rules[10] =
{ '\377', '\377', '\377', '\377', 'r', 'u', 'l', 'e', 's', '\0' };
char hl_info[9] =
{ '\377', '\377', '\377', '\377', 'i', 'n', 'f', 'o', '\0' };
char hl_players[12] =
{ '\377', '\377', '\377', '\377', 'p', 'l', 'a', 'y', 'e', 'r', 's', '\0' };
char hl_details[12] =
{ '\377', '\377', '\377', '\377', 'd', 'e', 't', 'a', 'i', 'l', 's', '\0' };

/* QUAKE WORLD MASTER */
#define QW_GET_SERVERS    'c'
char qw_masterquery[] = { QW_GET_SERVERS, '\n', '\0' };

/* HEXENWORLD MASTER */
#define HW_GET_SERVERS    'c'
char hw_masterquery[] = { '\377', HW_GET_SERVERS, '\0' };

/* QUAKE 2 MASTER */
char q2_masterquery[] = { 'q', 'u', 'e', 'r', 'y', '\n', '\0' };

/* QUAKE 3 MASTER */
char q3_master_query_template[] = "\377\377\377\377getservers %s %s";
char q3_master_default_protocol[] = "68";
char q3_master_default_query[] = "empty full demo\n";

/* RETURN TO CASTLE WOLFENSTEIN */
char rtcw_master_default_protocol[] = "60";

/* STAR TREK: ELITE FORCE */
char stef_master_default_protocol[] = "24";

/* JEDI KNIGHT: JEDI ACADEMY */
char jk3_master_default_protocol[] = "26";

/* HALF-LIFE MASTER */
char hl_masterquery[4] = { 'e', '\0', '\0', '\0' };
char new_hl_masterquery_prefix[5] = { '1', '\0', '\0', '\0', '\0' };

/* TRIBES */
char tribes_info[] = { '`', '*', '*' };
char tribes_players[] = { 'b', '*', '*' };
/*  This is what the game sends to get minimal status
{ '\020', '\03', '\377', 0, (unsigned char)0xc5, 6 };
*/
char tribes_info_reponse[] = { 'a', '*', '*', 'b' };
char tribes_players_reponse[] = { 'c', '*', '*', 'b' };
char tribes_masterquery[] = { 0x10, 0x3, '\377', 0, 0x2 };
char tribes_master_response[] = { 0x10, 0x6 };

/* GAMESPY */
char gamespy_master_request_prefix[] = "\\list\\\\gamename\\";
char gamespy_master_validate[] = "\\gamename\\gamespy2\\gamever\\020109017\\location\\5\\validate\\12345678\\final\\";

/* TRIBES 2 */
#define TRIBES2_QUERY_GAME_TYPES	2
#define TRIBES2_QUERY_MASTER		6
#define TRIBES2_QUERY_PING		14
#define TRIBES2_QUERY_INFO		18
#define TRIBES2_RESPONSE_GAME_TYPES	4
#define TRIBES2_RESPONSE_MASTER		8
#define TRIBES2_RESPONSE_PING		16
#define TRIBES2_RESPONSE_INFO		20

#define TRIBES2_NO_COMPRESS		2
#define TRIBES2_DEFAULT_PACKET_INDEX	255
#define TRIBES2_STATUS_DEDICATED	(1<<0)
#define TRIBES2_STATUS_PASSWORD		(1<<1)
#define TRIBES2_STATUS_LINUX		(1<<2)
#define TRIBES2_STATUS_TOURNAMENT	(1<<3)
#define TRIBES2_STATUS_NOALIAS		(1<<4)
#define TRIBES2_STATUS_TEAMDAMAGE	(1<<5)
#define TRIBES2_STATUS_TOURNAMENT_VER3	(1<<6)
#define TRIBES2_STATUS_NOALIAS_VER3	(1<<7)
char tribes2_game_types_request[] = { TRIBES2_QUERY_GAME_TYPES, 0, 1,2,3,4 };
char tribes2_ping[] = { TRIBES2_QUERY_PING, TRIBES2_NO_COMPRESS, 1,2,3,4 };
char tribes2_info[] = { TRIBES2_QUERY_INFO, TRIBES2_NO_COMPRESS, 1,2,3,4 };
unsigned char tribes2_masterquery[] = {
	TRIBES2_QUERY_MASTER, 128,	/* <= build 22228, this was 0 */
	11,12,13,14,
	255,
	3, 'a', 'n', 'y',
	3, 'a', 'n', 'y',
	0, 255,				/* min/max players */
	0xff, 0xff, 0xff, 0xff,		/* region mask */
	0, 0, 0, 0, 			/* build version */
	0,				/* status */
	255,				/* max bots */
	0, 0,				/* min cpu */
	0				/* # buddies */
	};
#define TRIBES2_ID_OFFSET	2

unsigned char descent3_masterquery[] = {
    0x03, /* ID or something */
    0x24, 0x00, 0x00, 0x00, /* packet len */
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* unknown */
    0x07, /* type */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* unknown */
    0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x00  /* "global" */
};

/* for some reason Descent3 uses a different request for pxo/non-pxo games. blah. */
unsigned char descent3_pxoinfoquery[] = {
    0x01, /* "internal descent3 routing" */
    0x29, /* request server info? (pxo listed servers) */
    0x0b, 0x00, /* packet length (- routing byte) */
    0x1b, 0x2f, 0xf4, 0x41, 0x09, 0x00, 0x00, 0x00 /* unknown */
};
unsigned char descent3_tcpipinfoquery[] = {
    0x01, /* "internal descent3 routing" */
    0x1e, /* request server info? (tcpip only servers) */
    0x0b, 0x00, /* packet length (- routing byte) */
    0x1b, 0x2f, 0xf4, 0x41, 0x09, 0x00, 0x00, 0x00 /* unknown */
};
/* http://ml.warpcore.org/d3dl/200101/msg00001.html
 * http://ml.warpcore.org/d3dl/200101/msg00004.html */
unsigned char descent3_playerquery[] = {
    0x01, /* "internal descent3 routing" */
    0x72, /* MP_REQUEST_PLAYERLIST   */
    0x03, 0x00 /* packet length (- routing byte) */
};

unsigned char ghostrecon_serverquery[] = {
	0xc0,0xde,0xf1,0x11,	/* const ? header */
	0x42,			/* start flag */
	0x03,0x00,		/* data len */
	0xe9,0x03,0x00		/* server request ?? */
};

unsigned char ghostrecon_playerquery[] = {
	0xc0,0xde,0xf1,0x11,		/* const ? header */
	0x42,				/* start flag */
	0x06,0x00,			/* data len */
	0xf5,0x03,0x00,0x78,0x30,0x63 /* player request ?? may be flag 0xf5; len 0x03,0x00; data 0x78, 0x30, 0x63 */
};

/* All Seeing Eye */
char eye_status_query[1]= "s";
char eye_ping_query[1]= "p";

// Gamespy v2
// Format:
// 1 - 3: query head
// 4 - 7: queryid
// 8: server + rules info (00 to disable)
// 9: Player information (00 to disable)
// 10: Team information (00 to disable)
unsigned char gs2_status_query[] = {
	0xfe,0xfd,0x00,0x10,0x20,0x30,0x40,0xff,0xff,0xff
};

// Gamespy v3
// Format:
// 1 - 3: query head
// 4 - 7: queryid
// 8: server + rules info (00 to disable)
// 9: Player information (00 to disable)
// 10: Team information (00 to disable)
// 11: Request new format
unsigned char gs3_player_query[] = {
	0xfe,0xfd,0x00,
	0x10,0x20,0x30,0x40,
	0xff,0xff,0xff,0x01
};

// Format:
// 1 - 3: query head
// 4 - 7: queryid
// 8: requested number of rules
// 9 - 9 + no_rules: requested ruleid
// last 2 : terminator?

// Known ruleid's:
// 0x01: hostname
// 0x03: version
// 0x04: hostport
// 0x05: map
// 0x06: gametype
// 0x07: gamevarient
// 0x08: num_players
// 0x0a: max_players
// 0x0b: gamemode
unsigned char gs3_status_query[] = {
	0xfe,0xfd,0x00,
	0x10,0x20,0x30,0x40,
	0x06,0x01,0x06,0x05,0x08,0x0a,0x04,0x00,0x00
};

// Gamespy v3 + challenge
// Format:
// 1 - 3: query head
// 4 - 7: queryid
unsigned char gs3_challenge[] = {
	0xfe,0xfd,0x09,
	0x10,0x20,0x30,0x40
};

// Format:
// 1 - 8: Query Request
// 9 - 12: Query Header
// 13: Query ID

// Query ID is made up of the following
// 0x01: Basic Info
// 0x02: Game Rules
// 0x03: Player Information
// 0x04: Team Information
unsigned char haze_status_query[] = {
	'f', 'r', 'd', 'q', 'u', 'e', 'r', 'y',
	0x10,0x20,0x30,0x40,
	0x0A
};

// Format:
// 1 - 8: Query Request
// 9 - 12: Query Header
// 13: Query ID

// Query ID is made up of the following
// 0x01: Basic Info
// 0x02: Game Rules
// 0x03: Player Information
// 0x04: Team Information
unsigned char haze_player_query[] = {
	'f', 'r', 'd', 'q', 'u', 'e', 'r', 'y',
	0x10,0x20,0x30,0x40,
	0x03
};


// Steam
// Format:
// 1. Request type ( 1 byte )
// 2. Region ( 1 byte )
// 3. ip ( string + null )
// 4. Filter ( optional + null )
//
// Regions:
// 0 = US East Coast
// 1 = US West Coast
// 2 = South America
// 3 = Europe
// 4 = Asia
// 5 = Australia
// 6 = Middle East
// 7 = Africa
// 255 = N/A
// Filter:
// \type\d = Returns only dedicated servers
//
// \secure\1 = Returns servers running anti-cheat technology
//
// \gamedir\[mod] = Servers running the specified modification.
// The parameter is the directory that the mod resides in e.g.
// cstrike for Counter-Strike or dod for Day of Defeat.
//
// \map\[map] = Returns servers running the specified map
// (e.g. de_dust2 or cs_italy)
//
// \linux\1 = Servers running on the Linux platform
//
// \empty\1 = Servers that are not empty
//
// \full\1 = Servers that are not full
//
// \proxy\1 = Servers that are spectator proxies
//
// End the filter with 0x00
//
unsigned char steam_masterquery_template[] = "1%c%s%c%s";

unsigned char savage_serverquery[] = {
	0x9e,0x4c,0x23,0x00,0x00,0xc8,0x01,0x21,0x00,0x00
};

unsigned char savage_playerquery[] = {
	0x9e,0x4c,0x23,0x00,0x00,0xce,0x76,0x46,0x00,0x00
};

unsigned char farcry_serverquery[] = {
	0x08,0x80
};

char ravenshield_serverquery[] = "REPORT";

unsigned char ts2_status_query[] = "si";

char ottd_master_query[] = {
	0x04, 0x00,	// packet length
	0x06,		// packet type
	0x01		// packet version
};

char ottd_serverinfo[] = {
	0x03, 0x00,	// packet length
	0x00,		// packet type
};

char ottd_serverdetails[] = {
	0x03, 0x00,	// packet length
	0x02,		// packet type
};

server_type builtin_types[] = {
{
    /* QUAKE */
    Q_SERVER,			/* id */
    "QS",			/* type_prefix */
    "qs",			/* type_string */
    "-qs",			/* type_option */
    "Quake",			/* game_name */
    0,				/* master */
    Q_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_RAW_STYLE_QUAKE,		/* flags */
    "*gamedir",			/* game_rule */
    "QUAKE",			/* template_var */
    (char*) &q_serverinfo,	/* status_packet */
    Q_SERVERINFO_LEN,		/* status_len */
    (char*) &q_player,		/* player_packet */
    Q_HEADER_LEN+1,		/* player_len */
    (char*) &q_rule,		/* rule_packet */
    sizeof( q_rule),		/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_q_packet,		/* packet_func */
},
{
    /* HEXEN 2 */
    H2_SERVER,			/* id */
    "H2S",			/* type_prefix */
    "h2s",			/* type_string */
    "-h2s",			/* type_option */
    "Hexen II",			/* game_name */
    0,				/* master */
    HEXEN2_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_RAW_STYLE_QUAKE,		/* flags */
    "*gamedir",			/* game_rule */
    "HEXEN2",			/* template_var */
    (char*) &h2_serverinfo,	/* status_packet */
    H2_SERVERINFO_LEN,		/* status_len */
    (char*) &q_player,		/* player_packet */
    Q_HEADER_LEN+1,		/* player_len */
    (char*) &q_rule,		/* rule_packet */
    sizeof( q_rule),		/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_q_packet,		/* packet_func */
},
{
    /* QUAKE WORLD */
    QW_SERVER,			/* id */
    "QWS",			/* type_prefix */
    "qws",			/* type_string */
    "-qws",			/* type_option */
    "QuakeWorld",		/* game_name */
    0,				/* master */
    QW_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "*gamedir",			/* game_rule */
    "QUAKEWORLD",		/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qw_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_qw_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_qw_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* HEXEN WORLD */
    HW_SERVER,			/* id */
    "HWS",			/* type_prefix */
    "hws",			/* type_string */
    "-hws",			/* type_option */
    "HexenWorld",		/* game_name */
    0,				/* master */
    HW_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "*gamedir",			/* game_rule */
    "HEXENWORLD",		/* template_var */
    (char*) &hw_serverstatus,	/* status_packet */
    sizeof( hw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qw_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_qw_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_qw_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* QUAKE 2 */
    Q2_SERVER,			/* id */
    "Q2S",			/* type_prefix */
    "q2s",			/* type_string */
    "-q2s",			/* type_option */
    "Quake II",			/* game_name */
    0,				/* master */
    Q2_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gamedir",			/* game_rule */
    "QUAKE2",			/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* QUAKE 3 */
    Q3_SERVER,			/* id */
    "Q3S",			/* type_prefix */
    "q3s",			/* type_string */
    "-q3s",			/* type_option */
    "Quake III: Arena",		/* game_name */
    0,				/* master */
    Q3_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_QUAKE3_NAMES /* TF_SINGLE_QUERY */,		/* flags */
    "game",			/* game_rule */
    "QUAKE3",			/* template_var */
    (char*) &q3_serverinfo,	/* status_packet */
    sizeof( q3_serverinfo),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &q3_serverstatus,	/* rule_packet */
    sizeof( q3_serverstatus),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* DOOM 3 */
    DOOM3_SERVER,					/* id */
    "DM3S",							/* type_prefix */
    "dm3s",							/* type_string */
    "-dm3s",						/* type_option */
    "Doom 3",						/* game_name */
    0,								/* master */
    DOOM3_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "fs_game",						/* game_rule */
    "DOOM3",						/* template_var */
    (char*) &doom3_serverinfo,		/* status_packet */
    sizeof( doom3_serverinfo),		/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_doom3_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_doom3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_doom3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_qwserver_request_packet,	/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_doom3_packet,			/* packet_func */
},
{
    /* HALFLIFE 2 */
    HL2_SERVER,						/* id */
    "HL2S",							/* type_prefix */
    "hl2s",							/* type_string */
    "-hl2s",						/* type_option */
    "Half-Life 2",					/* game_name */
    0,								/* master */
    HL2_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "",								/* game_rule */
    "HL2",							/* template_var */
    (char*) &hl2_serverinfo,		/* status_packet */
    sizeof( hl2_serverinfo),		/* status_len */
    (char*) &hl2_playerinfo,		/* player_packet */
    sizeof( hl2_playerinfo),		/* player_len */
    (char*) &hl2_ruleinfo,			/* rule_packet */
    sizeof( hl2_ruleinfo),			/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_hl2_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_hl2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_hl2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_hl2_request_packet,		/* status_query_func */
    NULL,				/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_hl2_packet,			/* packet_func */
},
{
    /* RETURN TO CASTLE WOLFENSTEIN */
    RTCW_SERVER,		/* id */
    "RWS",			/* type_prefix */
    "rws",			/* type_string */
    "-rws",			/* type_option */
    "Return to Castle Wolfenstein",	/* game_name */
    0,				/* master */
    RTCW_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_QUAKE3_NAMES /* TF_SINGLE_QUERY */,		/* flags */
    "game",			/* game_rule */
    "RTCW",			/* template_var */
    (char*) &q3_serverinfo,	/* status_packet */
    sizeof( q3_serverinfo),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &q3_serverstatus,	/* rule_packet */
    sizeof( q3_serverstatus),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* STAR TREK: ELITE FORCE */
    STEF_SERVER,		/* id */
    "EFS",			/* type_prefix */
    "efs",			/* type_string */
    "-efs",			/* type_option */
    "Star Trek: Elite Force",	/* game_name */
    0,				/* master */
    STEF_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_QUAKE3_NAMES /* TF_SINGLE_QUERY */,		/* flags */
    "game",			/* game_rule */
    "ELITEFORCE",		/* template_var */
    (char*) &q3_serverinfo,	/* status_packet */
    sizeof( q3_serverinfo),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &q3_serverstatus,	/* rule_packet */
    sizeof( q3_serverstatus),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* JEDI KNIGHT: JEDI ACADEMY */
    JK3_SERVER,		/* id */
    "JK3S",			/* type_prefix */
    "jk3s",			/* type_string */
    "-jk3s",			/* type_option */
    "Jedi Knight: Jedi Academy",	/* game_name */
    0,				/* master */
    JK3_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_QUAKE3_NAMES /* TF_SINGLE_QUERY */,		/* flags */
    "game",			/* game_rule */
    "JEDIKNIGHT3",		/* template_var */
    (char*) &q3_serverinfo,	/* status_packet */
    sizeof( q3_serverinfo),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &q3_serverstatus,	/* rule_packet */
    sizeof( q3_serverstatus),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* UNREAL TOURNAMENT 2003 */
    UT2003_SERVER,		/* id */
    "UT2S",			/* type_prefix */
    "ut2s",			/* type_string */
    "-ut2s",			/* type_option */
    "Unreal Tournament 2003",	/* game_name */
    0,				/* master */
    UNREAL_DEFAULT_PORT,	/* default_port */
    1,				/* port_offset */
    TF_U2_NAMES,				/* flags */
    "gametype",			/* game_rule */
    "UNREALTOURNAMENT2003",	/* template_var */
    (char*) &ut2003_basicstatus,/* status_packet */
    sizeof( ut2003_basicstatus),/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &ut2003_allinfo,	/* rule_packet */
    sizeof( ut2003_allinfo),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    0,				/* master_query */
    display_unreal_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_unreal_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_unreal_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_ut2003_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_ut2003_packet,	/* packet_func */
},
{
    /* UNREAL */
    UN_SERVER,			/* id */
    "UNS",			/* type_prefix */
    "uns",			/* type_string */
    "-uns",			/* type_option */
    "Unreal",			/* game_name */
    0,				/* master */
    UNREAL_DEFAULT_PORT,	/* default_port */
    1,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "UNREAL",			/* template_var */
    (char*) &unreal_serverstatus,	/* status_packet */
    sizeof( unreal_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_unreal_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_unreal_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_unreal_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gps_packet,	/* packet_func */
},
{
    /* HALF LIFE */
    HL_SERVER,			/* id */
    "HLS",			/* type_prefix */
    "hls",			/* type_string */
    "-hls",			/* type_option */
    "Half-Life",		/* game_name */
    0,				/* master */
    HALFLIFE_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "game",			/* game_rule */
    "HALFLIFE",			/* template_var */
    (char*) &hl_details,		/* status_packet */
    sizeof( hl_details),		/* status_len */
    (char*) &hl_players,	/* player_packet */
    sizeof( hl_players),	/* player_len */
    (char*) &hl_rules,		/* rule_packet */
    sizeof( hl_rules),		/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_halflife_player_info,/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_halflife_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_halflife_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_halflife_packet,	/* packet_func */
},
{
    /* SIN */
    SIN_SERVER,			/* id */
    "SNS",			/* type_prefix */
    "sns",			/* type_string */
    "-sns",			/* type_option */
    "Sin",			/* game_name */
    0,				/* master */
    SIN_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gamedir",			/* game_rule */
    "SIN",			/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* SHOGO */
    SHOGO_SERVER,		/* id */
    "SGS",			/* type_prefix */
    "sgs",			/* type_string */
    "-sgs",			/* type_option */
    "Shogo: Mobile Armor Division",	/* game_name */
    0,				/* master */
    SHOGO_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "",				/* game_rule */
    "SHOGO",			/* template_var */
    (char*) &unreal_serverstatus,	/* status_packet */
    sizeof( unreal_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gps_packet,	/* packet_func */
},
{
    /* TRIBES */
    TRIBES_SERVER,		/* id */
    "TBS",			/* type_prefix */
    "tbs",			/* type_string */
    "-tbs",			/* type_option */
    "Tribes",			/* game_name */
    0,				/* master */
    TRIBES_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "game",			/* game_rule */
    "TRIBES",			/* template_var */
    (char*) &tribes_info,	/* status_packet */
    sizeof( tribes_info),	/* status_len */
    (char*) &tribes_players,	/* player_packet */
    sizeof( tribes_players),	/* player_len */
    (char*) &tribes_players,	/* rule_packet */
    sizeof( tribes_players),	/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_tribes_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_tribes_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_tribes_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_tribes_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_tribes_packet,	/* packet_func */
},
{
    /* BFRIS */
    BFRIS_SERVER,		/* id */
    "BFS",			/* type_prefix */
    "bfs",			/* type_string */
    "-bfs",			/* type_option */
    "BFRIS",			/* game_name */
    0,				/* master */
    BFRIS_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_TCP_CONNECT,		/* flags */
    "Rules",			/* game_rule */
    "BFRIS",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_bfris_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_bfris_player_info,/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_bfris_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_bfris_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_bfris_packet,	/* packet_func */
},
{
    /* KINGPIN */
    KINGPIN_SERVER,		/* id */
    "KPS",			/* type_prefix */
    "kps",			/* type_string */
    "-kps",			/* type_option */
    "Kingpin",			/* game_name */
    0,				/* master */
    KINGPIN_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gamedir",			/* game_rule */
    "KINGPIN",			/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* HERETIC II */
    HERETIC2_SERVER,		/* id */
    "HRS",			/* type_prefix */
    "hrs",			/* type_string */
    "-hrs",			/* type_option */
    "Heretic II",		/* game_name */
    0,				/* master */
    HERETIC2_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gamedir",			/* game_rule */
    "HERETIC2",			/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* SOLDIER OF FORTUNE */
    SOF_SERVER,			/* id */
    "SFS",			/* type_prefix */
    "sfs",			/* type_string */
    "-sfs",			/* type_option */
    "Soldier of Fortune",	/* game_name */
    0,				/* master */
    SOF_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_SOF_NAMES,		/* flags */
    "gamedir",			/* game_rule */
    "SOLDIEROFFORTUNE",		/* template_var */
    (char*) &qw_serverstatus,	/* status_packet */
    sizeof( qw_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_q2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_qwserver_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qw_packet,	/* packet_func */
},
{
    /* GAMESPY PROTOCOL */
    GAMESPY_PROTOCOL_SERVER,	/* id */
    "GPS",			/* type_prefix */
    "gps",			/* type_string */
    "-gps",			/* type_option */
    "Gamespy Protocol",		/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_U2_NAMES,		/* flags */
    "gametype",			/* game_rule */
    "GAMESPYPROTOCOL",		/* template_var */
    (char*) &unreal_serverstatus,	/* status_packet */
    sizeof( unreal_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_unreal_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_unreal_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_unreal_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gps_packet,	/* packet_func */
},
{
    /* TRIBES 2 */
    TRIBES2_SERVER,		/* id */
    "T2S",			/* type_prefix */
    "t2s",			/* type_string */
    "-t2s",			/* type_option */
    "Tribes 2",			/* game_name */
    0,				/* master */
    TRIBES2_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "game",			/* game_rule */
    "TRIBES2",			/* template_var */
    (char*) &tribes2_ping,	/* status_packet */
    sizeof( tribes2_ping),	/* status_len */
    (char*) &tribes2_info,	/* player_packet */
    sizeof( tribes2_info),	/* player_len */
    (char*) NULL,		/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_tribes2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_tribes2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_tribes2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_tribes2_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_tribes2_packet,	/* packet_func */
},
{
    /* DESCENT3 GAMESPY PROTOCOL */
    DESCENT3_GAMESPY_SERVER,	/* id */
    "D3G",			/* type_prefix */
    "d3g",			/* type_string */
    "-d3g",			/* type_option */
    "Descent3 Gamespy Protocol",	/* game_name */
    0,				/* master */
    DESCENT3_GAMESPY_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "DESCENT3",			/* template_var */
    (char*) &unreal_serverstatus,	/* status_packet */
    sizeof( unreal_serverstatus),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_descent3_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_descent3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_descent3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gps_packet,	/* packet_func */
},
{
    /* DESCENT3 PROTOCOL */
    DESCENT3_SERVER,		/* id */
    "D3S",			/* type_prefix */
    "d3s",			/* type_string */
    "-d3s",			/* type_option */
    "Descent3",			/* game_name */
    0,				/* master */
    DESCENT3_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "gametype",			/* game_rule */
    "DESCENT3",			/* template_var */
    (char*) &descent3_tcpipinfoquery,	/* status_packet */
    sizeof( descent3_tcpipinfoquery),	/* status_len */
    (char*) &descent3_playerquery,	/* player_packet */
    sizeof( descent3_playerquery),	/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_descent3_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_descent3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_descent3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_descent3_packet,	/* packet_func */
},
{
    /* DESCENT3 PROTOCOL */
    DESCENT3_PXO_SERVER,	/* id */
    "D3P",			/* type_prefix */
    "d3p",			/* type_string */
    "-d3p",			/* type_option */
    "Descent3 PXO protocol",	/* game_name */
    0,				/* master */
    DESCENT3_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "gametype",			/* game_rule */
    "DESCENT3",			/* template_var */
    (char*) &descent3_pxoinfoquery,	/* status_packet */
    sizeof( descent3_pxoinfoquery),	/* status_len */
    (char*) &descent3_playerquery,	/* player_packet */
    sizeof( descent3_playerquery),	/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_descent3_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_descent3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_descent3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gps_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_descent3_packet,	/* packet_func */
},
{
    /* GHOSTRECON PROTOCOL */
    GHOSTRECON_SERVER,		/* id */
    "GRS",			/* type_prefix */
    "grs",			/* type_string */
    "-grs",			/* type_option */
    "Ghost Recon",		/* game_name */
    0,				/* master */
    GHOSTRECON_PLAYER_DEFAULT_PORT,	/* default_port */
    2,				/* port_offset */
    TF_QUERY_ARG,		/* flags */
    "gametype",			/* game_rule */
    "GHOSTRECON",			/* template_var */
    (char*) &ghostrecon_playerquery,	/* status_packet */
    sizeof( ghostrecon_playerquery),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_ghostrecon_player_info,	/* display_player_func */
    display_server_rules,		/* display_rule_func */
    raw_display_ghostrecon_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_ghostrecon_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_ghostrecon_request_packet,	/* status_query_func */
    NULL,				/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_ghostrecon_packet,	/* packet_func */
},
{
    /* ALL SEEING EYE PROTOCOL */
    ALLSEEINGEYE_PROTOCOL_SERVER,	/* id */
    "EYE",			/* type_prefix */
    "eye",			/* type_string */
    "-eye",			/* type_option */
    "All Seeing Eye Protocol",	/* game_name */
    0,				/* master */
    0,				/* default_port */
    123,			/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",				/* game_rule */
    "EYEPROTOCOL",		/* template_var */
    (char*) &eye_status_query,	/* status_packet */
    sizeof( eye_status_query),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_eye_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_eye_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_eye_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_eye_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_eye_packet,	/* packet_func */
},
{
    /* GAMESPY V2 PROTOCOL */
    GAMESPY2_PROTOCOL_SERVER,	/* id */
    "GS2",			/* type_prefix */
    "gs2",			/* type_string */
    "-gs2",			/* type_option */
    "Gamespy V2 Protocol",	/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "GPS2PROTOCOL",		/* template_var */
    (char*) &gs2_status_query,	/* status_packet */
    sizeof( gs2_status_query),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_gs2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_gs2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_gs2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gs2_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gs2_packet,	/* packet_func */
},
{
    /* AMERICA'S ARMY EXTENSION (GS2 BASED) */
    ARMYOPS_SERVER,		/* id */
    "AMS",			/* type_prefix */
    "ams",			/* type_string */
    "-ams",			/* type_option */
    "America's Army v2.x",	/* game_name */
    0,				/* master */
    1716,			/* default_port */
    1,				/* port_offset */
    TF_SINGLE_QUERY|TF_U2_NAMES,	/* flags */
    "gametype",			/* game_rule */
    "AMERICASARMY",		/* template_var */
    (char*) &gs2_status_query,	/* status_packet */
    sizeof( gs2_status_query),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_armyops_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_armyops_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_armyops_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gs2_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gs2_packet,	/* packet_func */
},
{
    /* RAVENSHIELD PROTOCOL */
    RAVENSHIELD_SERVER,		/* id */
    "RSS",			/* type_prefix */
    "rss",			/* type_string */
    "-rss",			/* type_option */
    "Ravenshield",		/* game_name */
    0,				/* master */
    RAVENSHIELD_DEFAULT_PORT,	/* default_port */
    1000,				/* port_offset */
    TF_QUERY_ARG,		/* flags */
    "gametype",			/* game_rule */
    "RAVENSHIELD",			/* template_var */
    (char*)ravenshield_serverquery,			/* status_packet */
    sizeof( ravenshield_serverquery ) - 1,	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_ravenshield_player_info,	/* display_player_func */
    display_server_rules,		/* display_rule_func */
    raw_display_ravenshield_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_ravenshield_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_ravenshield_request_packet,	/* status_query_func */
    NULL,				/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_ravenshield_packet,	/* packet_func */
},
{
    /* SAVAGE PROTOCOL */
    SAVAGE_SERVER,		/* id */
    "SAS",			/* type_prefix */
    "sas",			/* type_string */
    "-sas",			/* type_option */
    "Savage",		/* game_name */
    0,				/* master */
   	SAVAGE_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_QUERY_ARG,		/* flags */
    "gametype",			/* game_rule */
    "SAVAGE",			/* template_var */
    (char*)savage_serverquery,			/* status_packet */
    sizeof( savage_serverquery ) - 1,	/* status_len */
    (char*)savage_playerquery,			/* player_packet */
    sizeof( savage_playerquery ) - 1,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_savage_player_info,	/* display_player_func */
    display_server_rules,		/* display_rule_func */
    raw_display_savage_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_savage_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_savage_request_packet,	/* status_query_func */
    NULL,				/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_savage_packet,	/* packet_func */
},
{
    /* FARCRY PROTOCOL */
    FARCRY_SERVER,		/* id */
    "FCS",			/* type_prefix */
    "fcs",			/* type_string */
    "-fcs",			/* type_option */
    "FarCry",		/* game_name */
    0,				/* master */
   	FARCRY_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_QUERY_ARG,		/* flags */
    "gametype",			/* game_rule */
    "FARCRY",			/* template_var */
    (char*)farcry_serverquery,			/* status_packet */
    sizeof( savage_serverquery ) - 1,	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_farcry_player_info,	/* display_player_func */
    display_server_rules,		/* display_rule_func */
    raw_display_farcry_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_farcry_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_farcry_request_packet,	/* status_query_func */
    NULL,				/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_farcry_packet,	/* packet_func */
},


/* --- MASTER SERVERS --- */
{
    /* QUAKE WORLD MASTER */
    QW_MASTER,			/* id */
    "QWM",			/* type_prefix */
    "qwm",			/* type_string */
    "-qwm",			/* type_option */ /* ## also "-qw" */
    "QuakeWorld Master",	/* game_name */
    QW_SERVER,			/* master */
    QW_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_OUTFILE,	/* flags */
    "",				/* game_rule */
    "QWMASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &qw_masterquery,	/* master_packet */
    sizeof( qw_masterquery),	/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* HEXEN2WORLD MASTER */
    HW_MASTER,			/* id */
    "HWM",			/* type_prefix */
    "hwm",			/* type_string */
    "-hwm",			/* type_option */ /* ## also "-qw" */
    "HexenWorld Master",	/* game_name */
    HW_SERVER,			/* master */
    HW_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_OUTFILE,	/* flags */
    "",				/* game_rule */
    "HWMASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &hw_masterquery,	/* master_packet */
    sizeof( hw_masterquery),	/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* QUAKE 2 MASTER */
    Q2_MASTER,			/* id */
    "Q2M",			/* type_prefix */
    "q2m",			/* type_string */
    "-q2m",			/* type_option */ /* ## also "-qw" */
    "Quake II Master",		/* game_name */
    Q2_SERVER,			/* master */
    Q2_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_OUTFILE,	/* flags */
    "",				/* game_rule */
    "Q2MASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    q2_masterquery,		/* master_packet */
    sizeof( q2_masterquery),	/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* QUAKE 3 MASTER */
    Q3_MASTER,			/* id */
    "Q3M",			/* type_prefix */
    "q3m",			/* type_string */
    "-q3m",			/* type_option */
    "Quake III Master",		/* game_name */
    Q3_SERVER,			/* master */
    Q3_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "Q3MASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    q3_master_query_template,	/* master_packet */
    0,				/* master_len */
    q3_master_default_protocol,	/* master_protocol */
    q3_master_default_query,	/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* DOOM 3 MASTER */
    DOOM3_MASTER,			/* id */
    "DM3M",			/* type_prefix */
    "dm3m",			/* type_string */
    "-dm3m",			/* type_option */
    "Doom 3 Master",		/* game_name */
    DOOM3_SERVER,			/* master */
    DOOM3_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE|TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "DOOM3MASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,	/* master_packet */
    0,          /* master_len */
    NULL,	/* master_protocol */
    NULL,	/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_doom3master_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_doom3master_packet,	/* packet_func */
},
{
    /* RETURN TO CASTLE WOLFENSTEIN MASTER */
    RTCW_MASTER,		/* id */
    "RWM",			/* type_prefix */
    "rwm",			/* type_string */
    "-rwm",			/* type_option */
    "Return to Castle Wolfenstein Master",	/* game_name */
    RTCW_SERVER,		/* master */
    RTCW_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "RTCWMASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    q3_master_query_template,	/* master_packet */
    0,				/* master_len */
    rtcw_master_default_protocol,	/* master_protocol */
    q3_master_default_query,		/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* STAR TREK: ELITE FORCE MASTER */
    STEF_MASTER,		/* id */
    "EFM",			/* type_prefix */
    "efm",			/* type_string */
    "-efm",			/* type_option */
    "Star Trek: Elite Force",	/* game_name */
    STEF_SERVER,		/* master */
    STEF_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "ELITEFORCEMASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    q3_master_query_template,	/* master_packet */
    0,				/* master_len */
    stef_master_default_protocol,	/* master_protocol */
    q3_master_default_query,		/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* JEDI KNIGHT 3 FORCE MASTER */
    JK3_MASTER,		/* id */
    "JK3M",			/* type_prefix */
    "jk3m",			/* type_string */
    "-jk3m",			/* type_option */
    "Jedi Knight: Jedi Academy",	/* game_name */
    JK3_SERVER,		/* master */
    JK3_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "JK3MASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    q3_master_query_template,	/* master_packet */
    0,				/* master_len */
    jk3_master_default_protocol,	/* master_protocol */
    NULL,		/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* HALF-LIFE MASTER */
    HL_MASTER,			/* id */
    "HLM",			/* type_prefix */
    "hlm",			/* type_string */
    "-hlm",			/* type_option */ /* ## also "-qw" */
    "Half-Life Master",		/* game_name */
    HL_SERVER,			/* master */
    HL_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_OUTFILE|TF_QUERY_ARG, /* flags */
    "",				/* game_rule */
    "HLMASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &new_hl_masterquery_prefix,	/* master_packet */
    sizeof( new_hl_masterquery_prefix),	/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},
{
    /* TRIBES MASTER */
    TRIBES_MASTER,		/* id */
    "TBM",			/* type_prefix */
    "tbm",			/* type_string */
    "-tbm",			/* type_option */
    "Tribes Master",		/* game_name */
    TRIBES_SERVER,		/* master */
    TRIBES_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE,			/* flags */
    "",				/* game_rule */
    "TRIBESMASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &tribes_masterquery,/* master_packet */
    sizeof( tribes_masterquery),/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_tribesmaster_packet,/* packet_func */
},
{
    /* GAMESPY MASTER */
    GAMESPY_MASTER,		/* id */
    "GSM",			/* type_prefix */
    "gsm",			/* type_string */
    "-gsm",			/* type_option */
    "Gamespy Master",		/* game_name */
    GAMESPY_PROTOCOL_SERVER,	/* master */
    GAMESPY_MASTER_DEFAULT_PORT,		/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_TCP_CONNECT | TF_QUERY_ARG | TF_QUERY_ARG_REQUIRED,	/* flags */
    "",				/* game_rule */
    "GAMESPYMASTER",		/* template_var */
    (char*) &gamespy_master_request_prefix,	/* status_packet */
    sizeof( gamespy_master_request_prefix)-1,	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &gamespy_master_validate,/* master_packet */
    sizeof( gamespy_master_validate)-1,/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_gamespy_master_request,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gamespy_master_response,/* packet_func */
},
{
    /* TRIBES 2 MASTER */
    TRIBES2_MASTER,		/* id */
    "T2M",			/* type_prefix */
    "t2m",			/* type_string */
    "-t2m",			/* type_option */
    "Tribes 2 Master",		/* game_name */
    TRIBES2_SERVER,		/* master */
    TRIBES2_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "TRIBES2MASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &tribes2_masterquery,/* master_packet */
    sizeof( tribes2_masterquery),/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_tribes2master_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_tribes2master_packet,/* packet_func */
},
{
    /* DESCENT3 MASTER */
    DESCENT3_MASTER,		/* id */
    "D3M",			/* type_prefix */
    "d3m",			/* type_string */
    "-d3m",			/* type_option */ /* ## also "-qw" */
    "Descent3 Master (PXO)",	/* game_name */
    DESCENT3_PXO_SERVER,	/* master */
    DESCENT3_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_MASTER_MULTI_RESPONSE|TF_OUTFILE,	/* flags */
    "",				/* game_rule */
    "D3MASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*)descent3_masterquery,	/* master_packet */
    sizeof( descent3_masterquery),	/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_descent3master_packet,	/* packet_func */
},
{
    /* STEAM MASTER */
    STEAM_MASTER,			/* id */
    "STM",			/* type_prefix */
    "stm",			/* type_string */
    "-stm",			/* type_option */ /* ## also "-qw" */
    "Steam Master",		/* game_name */
    A2S_SERVER,			/* master */
    STEAM_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY|TF_OUTFILE|TF_QUERY_ARG|TF_MASTER_STEAM, /* flags */
    "",				/* game_rule */
    "STEAMMASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) &steam_masterquery_template,	/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_qwmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_qwmaster_packet,	/* packet_func */
},

{
    /* UT2004 MASTER */
    UT2004_MASTER,		/* id */
    "UT2004M",			/* type_prefix */
    "ut2004m",			/* type_string */
    "-ut2004m",			/* type_option */
    "UT2004 Master",		/* game_name */
    UT2003_SERVER,		/* master */
    28902,			/* default_port */
    0,				/* port_offset */
    TF_OUTFILE|TF_QUERY_ARG|TF_TCP_CONNECT,	/* flags */
    "",				/* game_rule */
    "UT2004MASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,	/* master_protocol */
    NULL,	/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_ut2004master_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_ut2004master_packet,	/* packet_func */
},
{
    /* HALFLIFE 2 */
    A2S_SERVER,						/* id */
    "A2S",							/* type_prefix */
    "a2s",							/* type_string */
    "-a2s",						/* type_option */
    "Half-Life 2 new",					/* game_name */
    0,								/* master */
    HL2_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "gamedir",								/* game_rule */
    "A2S",							/* template_var */
    NULL,		/* status_packet */
    0,		/* status_len */
    NULL,		/* player_packet */
    0,		/* player_len */
    NULL,			/* rule_packet */
    0,			/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_hl2_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_hl2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_hl2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_a2s_request_packet,		/* status_query_func */
    send_a2s_rule_request_packet,	/* rule_query_func */
    NULL,				/* player_query_func */
    deal_with_a2s_packet,		/* packet_func */
},
{
    /* PARIAH */
    PARIAH_SERVER,	/* id */
    "PRS",			/* type_prefix */
    "prs",			/* type_string */
    "-prs",			/* type_option */
    "Pariah",		/* game_name */
    0,				/* master */
    UNREAL_DEFAULT_PORT,	/* default_port */
    1,				/* port_offset */
    TF_U2_NAMES,				/* flags */
    "gametype",			/* game_rule */
    "UNREALTOURNAMENT2003",	/* template_var */
    (char*) &pariah_basicstatus,/* status_packet */
    sizeof( pariah_basicstatus),/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    0,				/* master_query */
    display_unreal_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_unreal_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_unreal_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_ut2003_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_ut2003_packet,	/* packet_func */
},
{
    /* GAMESPY V3 PROTOCOL */
    GAMESPY3_PROTOCOL_SERVER,	/* id */
    "GS3",			/* type_prefix */
    "gs3",			/* type_string */
    "-gs3",			/* type_option */
    "Gamespy V3 Protocol",	/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "GPS3PROTOCOL",		/* template_var */
    (char*) &gs3_status_query,	/* status_packet */
    sizeof( gs3_status_query),	/* status_len */
    (char*) &gs3_player_query,	/* player_packet */
    sizeof( gs3_player_query),	/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_gs2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_gs2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_gs2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gs3_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gs3_packet,	/* packet_func */
},
{
    /* TEAMSPEAK 2 PROTOCOL */
    TS2_PROTOCOL_SERVER,			/* id */
    "TS2",							/* type_prefix */
    "ts2",							/* type_string */
    "-ts2",							/* type_option */
    "Teamspeak 2",					/* game_name */
    0,								/* master */
    0,								/* default_port */
    0,								/* port_offset */
    TF_TCP_CONNECT|TF_QUERY_ARG_REQUIRED|TF_QUERY_ARG,	/* flags */
    "N/A",							/* game_rule */
    "TS2PROTOCOL",					/* template_var */
    NULL,							/* status_packet */
    0,								/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_ts2_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_ts2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_ts2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_ts2_request_packet,		/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_ts2_packet,			/* packet_func */
},
{
    /* QUAKE 4 */
    QUAKE4_SERVER,					/* id */
    "Q4S",							/* type_prefix */
    "q4s",							/* type_string */
    "-q4s",							/* type_option */
    "Quake 4",						/* game_name */
    0,								/* master */
    QUAKE4_DEFAULT_PORT,			/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "fs_game",						/* game_rule */
    "QUAKE4",						/* template_var */
    (char*) &doom3_serverinfo,		/* status_packet */
    sizeof( doom3_serverinfo),		/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_doom3_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_doom3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_doom3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_qwserver_request_packet,	/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_quake4_packet,		/* packet_func */
},
{
    /* QUAKE 4 MASTER */
    QUAKE4_MASTER,		/* id */
    "Q4M",			/* type_prefix */
    "q4m",			/* type_string */
    "-q4m",			/* type_option */
    "Quake 4 Master",		/* game_name */
    QUAKE4_SERVER,		/* master */
    QUAKE4_MASTER_DEFAULT_PORT,	/* default_port */
    0,				/* port_offset */
    TF_OUTFILE|TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "QUAKE4MASTER",			/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,	/* master_packet */
    0,          /* master_len */
    NULL,	/* master_protocol */
    NULL,	/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_quake4master_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_doom3master_packet,	/* packet_func */
},
{
    /* GAMESPY V4 PROTOCOL */
    GAMESPY4_PROTOCOL_SERVER,	/* id */
    "GS4",			/* type_prefix */
    "gs4",			/* type_string */
    "-gs4",			/* type_option */
    "Gamespy V4 Protocol",	/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "GPS4PROTOCOL",		/* template_var */
    (char*) &gs3_challenge,	/* status_packet */
    sizeof( gs3_challenge),	/* status_len */
    (char*) &gs3_challenge,	/* player_packet */
    sizeof( gs3_challenge),	/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_gs2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_gs2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_gs2_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_gs3_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_gs3_packet,	/* packet_func */
},
{
    /* PREY */
    PREY_SERVER,					/* id */
    "PREYS",							/* type_prefix */
    "preys",							/* type_string */
    "-preys",						/* type_option */
    "PREY",						/* game_name */
    0,								/* master */
    PREY_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "fs_game",						/* game_rule */
    "PREY",						/* template_var */
    (char*) &doom3_serverinfo,		/* status_packet */
    sizeof( doom3_serverinfo),		/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_doom3_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_doom3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_doom3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_qwserver_request_packet,	/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_prey_packet,			/* packet_func */
},
{
    /* TRACKMANIA PROTOCOL */
    TM_PROTOCOL_SERVER,			/* id */
    "TM",							/* type_prefix */
    "tm",							/* type_string */
    "-tm",							/* type_option */
    "TrackMania",					/* game_name */
    0,								/* master */
    0,								/* default_port */
    0,								/* port_offset */
    TF_TCP_CONNECT|TF_QUERY_ARG|TF_TM_NAMES,	/* flags */
    "N/A",							/* game_rule */
    "TMPROTOCOL",					/* template_var */
    NULL,							/* status_packet */
    0,								/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_tm_player_info,			/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_tm_player_info,		/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_tm_player_info,		/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_tm_request_packet,			/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_tm_packet,			/* packet_func */
},
{
    /* Enemy Territory : QuakeWars */
    ETQW_SERVER,					/* id */
    "ETQWS",						/* type_prefix */
    "etqws",						/* type_string */
    "-etqws",						/* type_option */
    "QuakeWars",					/* game_name */
    0,								/* master */
    ETQW_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "fs_game",						/* game_rule */
    "QUAKE4",						/* template_var */
    (char*) &doom3_serverinfo,		/* status_packet */
    sizeof( doom3_serverinfo),		/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_doom3_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_doom3_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_doom3_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_qwserver_request_packet,	/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_etqw_packet,			/* packet_func */
},
{
    /* HAZE PROTOCOL */
    HAZE_SERVER,	/* id */
    "HAZES",			/* type_prefix */
    "hazes",			/* type_string */
    "-hazes",			/* type_option */
    "Haze Protocol",	/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    TF_SINGLE_QUERY,		/* flags */
    "gametype",			/* game_rule */
    "HAZE",		/* template_var */
    (char*) &haze_status_query,	/* status_packet */
    sizeof( haze_status_query),	/* status_len */
    (char*) &haze_player_query,	/* player_packet */
    sizeof( haze_player_query),	/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_haze_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_haze_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_haze_player_info,	/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_haze_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_haze_packet,	/* packet_func */
},
{
    /* World in Confict PROTOCOL */
    WIC_PROTOCOL_SERVER,			/* id */
    "WICS",							/* type_prefix */
    "wics",							/* type_string */
    "-wics",						/* type_option */
    "World in Conflict",			/* game_name */
    0,								/* master */
    0,								/* default_port */
    0,								/* port_offset */
    TF_TCP_CONNECT|TF_QUERY_ARG_REQUIRED|TF_QUERY_ARG,	/* flags */
    "N/A",							/* game_rule */
    "WICPROTOCOL",					/* template_var */
    NULL,							/* status_packet */
    0,								/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_wic_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_wic_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_wic_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_wic_request_packet,		/* status_query_func */
    NULL,							/* rule_query_func */
    NULL,							/* player_query_func */
    deal_with_wic_packet,			/* packet_func */
},
{
    /* openTTD */
    OTTD_SERVER,			/* id */
    "OTTDS",			/* type_prefix */
    "ottds",			/* type_string */
    "-ottds",			/* type_option */
    "OpenTTD",			/* game_name */
    0,				/* master */
    3979,			/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "",				/* game_rule */
    "OPENTTD",			/* template_var */
    (char*) &ottd_serverinfo,	/* status_packet */
    sizeof( ottd_serverinfo),	/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    (char*) &ottd_serverdetails,/* rule_packet */
    sizeof( ottd_serverdetails),   /* rule_len */
    NULL,			/* master_packet */
    0,				/* master_len */
    NULL,			/* master_protocol */
    NULL,			/* master_query */
    display_q2_player_info,	/* display_player_func */
    display_server_rules,	/* display_rule_func */
    raw_display_q2_player_info,	/* display_raw_player_func */
    raw_display_server_rules,	/* display_raw_rule_func */
    xml_display_gs2_player_info,/* display_xml_player_func */
    xml_display_server_rules,	/* display_xml_rule_func */
    send_ottd_request_packet,	/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_ottd_packet,	/* packet_func */
},
{
    /* openTTD Master */
    OTTD_MASTER,		/* id */
    "OTTDM",			/* type_prefix */
    "ottdm",			/* type_string */
    "-ottdm",			/* type_option */
    "openTTD Master",		/* game_name */
    OTTD_SERVER,		/* master */
    3978,			/* default_port */
    0,				/* port_offset */
    TF_OUTFILE | TF_QUERY_ARG,	/* flags */
    "",				/* game_rule */
    "OTTDMASTER",		/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    ottd_master_query,	/* master_packet */
    sizeof(ottd_master_query),/* master_len */
    NULL,	/* master_protocol */
    NULL,	/* master_query */
    display_qwmaster,		/* display_player_func */
    NULL,	/* display_rule_func */
    NULL,	/* display_raw_player_func */
    NULL,	/* display_raw_rule_func */
    NULL,	/* display_xml_player_func */
    NULL,	/* display_xml_rule_func */
    send_ottdmaster_request_packet,/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    deal_with_ottdmaster_packet,	/* packet_func */
},
{
    /* Frontlines-Fuel of War */
    FL_SERVER,						/* id */
    "FLS",							/* type_prefix */
    "fls",							/* type_string */
    "-fls",							/* type_option */
    "Frontlines-Fuel of War",		/* game_name */
    0,								/* master */
    FL_DEFAULT_PORT,				/* default_port */
    0,								/* port_offset */
    TF_QUAKE3_NAMES,				/* flags */
    "gamedir",						/* game_rule */
    "FLS",							/* template_var */
    NULL,							/* status_packet */
    0,								/* status_len */
    NULL,							/* player_packet */
    0,								/* player_len */
    NULL,							/* rule_packet */
    0,								/* rule_len */
    NULL,							/* master_packet */
    0,								/* master_len */
    NULL,							/* master_protocol */
    NULL,							/* master_query */
    display_fl_player_info,		/* display_player_func */
    display_server_rules,			/* display_rule_func */
    raw_display_fl_player_info,	/* display_raw_player_func */
    raw_display_server_rules,		/* display_raw_rule_func */
    xml_display_fl_player_info,	/* display_xml_player_func */
    xml_display_server_rules,		/* display_xml_rule_func */
    send_fl_request_packet,			/* status_query_func */
    send_fl_rule_request_packet,	/* rule_query_func */
	NULL,							/* player_query_func */
    deal_with_fl_packet,			/* packet_func */
},
{
    Q_UNKNOWN_TYPE,		/* id */
    "",				/* type_prefix */
    "",				/* type_string */
    "",				/* type_option */
    "",				/* game_name */
    0,				/* master */
    0,				/* default_port */
    0,				/* port_offset */
    0,				/* flags */
    "",				/* game_rule */
    "",				/* template_var */
    NULL,			/* status_packet */
    0,				/* status_len */
    NULL,			/* player_packet */
    0,				/* player_len */
    NULL,			/* rule_packet */
    0,				/* rule_len */
    (char*) NULL,		/* master_packet */
    0,				/* master_len */
    NULL,			/* display_player_func */
    NULL,			/* display_rule_func */
    NULL,			/* display_raw_player_func */
    NULL,			/* display_raw_rule_func */
    NULL,			/* display_xml_player_func */
    NULL,			/* display_xml_rule_func */
    NULL,			/* status_query_func */
    NULL,			/* rule_query_func */
    NULL,			/* player_query_func */
    NULL,			/* packet_func */
}
};

#endif /* QUERY_PACKETS */

/* Structures for keeping information about Quake servers, server
 * rules, and players.
 */

struct player;

#define FLAG_BROADCAST		(1<<1)
#define FLAG_PLAYER_TEAMS	(1<<2)
#define FLAG_DO_NOT_FREE_GAME	(1<<3)

#define PLAYER_TYPE_NORMAL	1
#define PLAYER_TYPE_BOT		2
#define PLAYER_TYPE_ALIAS	4

#define PLAYER_FLAG_DO_NOT_FREE_TEAM	1

struct player  {
    int number;
    char *name;
    int frags;
    int team;		/* Unreal and Tribes only */
    char *team_name;	/* Tribes, BFRIS only, do not free()  */
    int connect_time;
    int shirt_color;
    int pants_color;
    char *address;
    int ping;
    short flags;
    short type_flag;	/* Tribes 2 only */
    int packet_loss;	/* Tribes only */
    char *tribe_tag;	/* Tribes 2 / Quake 4 clan name */
    char *skin;
    char *mesh;		/* Unreal only */
    char *face;		/* Unreal only */
    int score;		/* BFRIS only */
    int ship;		/* BFRIS only */
    int room;		/* BFRIS only */
    int deaths;		/* Descent3 only */

	char *next_info;
	int n_info;
	struct info *info;
	struct info **last_info;
	int missing_rules;

    struct player *next;
};

struct rule  {
    char *name;
    char *value;
    struct rule *next;
};

struct info  {
    char *name;
    char *value;
    struct info *next;
};

extern char *qstat_version;
extern char *DOWN;
extern char *SYSERROR;
extern char *TIMEOUT;
extern char *MASTER;
extern char *SERVERERROR;
extern char *HOSTNOTFOUND;

extern int n_retries;

extern struct timeval packet_recv_time;

#define DEFAULT_RETRIES			3
#define DEFAULT_RETRY_INTERVAL		500	/* milli-seconds */
#define MAXFD_DEFAULT			20

#define SORT_GAME		1
#define SORT_PING		2
extern int first_sort_key;
extern int second_sort_key;

#define SECONDS 0
#define CLOCK_TIME 1
#define STOPWATCH_TIME 2
#define DEFAULT_TIME_FMT_RAW            SECONDS
#define DEFAULT_TIME_FMT_DISPLAY        CLOCK_TIME
extern int time_format;

extern int color_names;
extern int show_errors;

extern int get_player_info;
extern int get_server_rules;
extern int no_port_offset;

/* Definitions for the original Quake network protocol.
 */

#define PACKET_LEN 0xffff

/* Quake packet formats and magic numbers
 */
struct qheader  {
    unsigned char flag1;
    unsigned char flag2;
    unsigned short length;
    unsigned char op_code;
};

#define Q_NET_PROTOCOL_VERSION	3
#define HEXEN2_NET_PROTOCOL_VERSION	4

#define Q_CCREQ_CONNECT		0x01
#define Q_CCREP_ACCEPT		0x81
#define Q_CCREP_REJECT		0x82

#define Q_CCREP_SERVER_INFO	0x83

#define Q_CCREP_PLAYER_INFO	0x84

#define Q_CCREP_RULE_INFO	0x85

#define Q_DEFAULT_SV_MAXSPEED	"320"
#define Q_DEFAULT_SV_FRICTION	"4"
#define Q_DEFAULT_SV_GRAVITY	"800"
#define Q_DEFAULT_NOEXIT	"0"
#define Q_DEFAULT_TEAMPLAY	"0"
#define Q_DEFAULT_TIMELIMIT	"0"
#define Q_DEFAULT_FRAGLIMIT	"0"


/* Definitions for the QuakeWorld network protocol
 */


/*
#define QW_GET_SERVERS    'c'
*/
#define QW_SERVERS        'd'
#define HL_SERVERS        'f'
/*
HL master: send 'a', master responds with a small 'l' packet containing
	the text "Outdated protocol"
HL master: send 'e', master responds with a small 'f' packet
HL master: send 'g', master responds with a small 'h' packet containing
	name of master server
HL master: send 'i', master responds with a small 'j' packet
*/
#define QW_GET_USERINFO   'o'
#define QW_USERINFO       'p'
#define QW_GET_SEENINFO   'u'
#define QW_SEENINFO       'v'
#define QW_NACK           'm'
#define QW_NEWLINE        '\n'
#define QW_RULE_SEPARATOR '\\'

#define QW_REQUEST_LENGTH 20

int is_default_rule( struct rule *rule);
char *xform_name( char*, struct qserver *server);
char *quake_color( int color);
char *play_time( int seconds, int show_seconds);
char *ping_time( int ms);
char *get_qw_game( struct qserver *server);


/*
 * Query status and packet handling functions
 */

int cleanup_qserver( struct qserver *server, int force);
void change_server_port( struct qserver *server, unsigned short port, int force );

int server_info_packet( struct qserver *server, struct q_packet *pkt, int datalen );
int player_info_packet( struct qserver *server, struct q_packet *pkt, int datalen );
int rule_info_packet( struct qserver *server, struct q_packet *pkt, int datalen );

int time_delta( struct timeval *later, struct timeval *past);
char * strherror( int h_err);
int connection_refused();
int connection_would_block();
int connection_reset();

void add_file( char *filename);
int add_qserver( char *arg, server_type* type, char *outfilename, char *query_arg);
struct qserver* add_qserver_byaddr( unsigned int ipaddr, unsigned short port, server_type* type, int *new_server);
void init_qserver( struct qserver *server, server_type* type);
int bind_qserver( struct qserver *server);
int bind_sockets();
void send_packets();
struct qserver * find_server_by_address( unsigned int ipaddr, unsigned short port);
void add_server_to_hash( struct qserver *server);

#define NO_FLAGS 0
#define NO_VALUE_COPY 1
#define CHECK_DUPLICATE_RULES 2
#define NO_KEY_COPY 4
#define COMBINE_VALUES 8
#define OVERWITE_DUPLICATES 16

struct player* get_player_by_number( struct qserver *server, int player_number );
struct rule* add_rule( struct qserver *server, char *key, char *value,	int flags) ;
struct player* add_player( struct qserver *server, int player_number );
struct info* player_add_info( struct player *player, char *key, char *value, int flags );
void players_set_teamname( struct qserver *server, int teamid, char *teamname );


/*
 * Output template stuff
 */

int read_qserver_template( char *filename);
int read_rule_template( char *filename);
int read_header_template( char *filename);
int read_trailer_template( char *filename);
int read_player_template( char *filename);
int have_server_template();
int have_header_template();
int have_trailer_template();

void template_display_server( struct qserver *server);
void template_display_header();
void template_display_trailer();
void template_display_players( struct qserver *server);
void template_display_player( struct qserver *server, struct player *player);
void template_display_rules( struct qserver *server);
void template_display_rule( struct qserver *server, struct rule *rule);



/*
 * Host cache stuff
 */

int hcache_open( char *filename, int update);
void hcache_write( char *filename);
void hcache_invalidate();
void hcache_validate();
unsigned long hcache_lookup_hostname( char *hostname);
char * hcache_lookup_ipaddr( unsigned long ipaddr);
void hcache_write_file( char *filename);
void hcache_update_file();

unsigned int swap_long_from_little( void *l);
unsigned short swap_short_from_little( void *l);
float swap_float_from_little( void *f);

/** \brief write four bytes in little endian order */
void put_long_little(unsigned val, char* buf);

/*
 * Exported Globals
 */
extern int show_game_port;

#define NA_INT -32767
#define NO_PLAYER_INFO 0xffff
#define NO_SERVER_RULES NULL

#define FORCE 1
#define NO_FORCE 0

#endif
