/*
 * qstat 2.6
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Copyright 1996,1997,1998,1999 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"

#ifdef __hpux
#define STATIC static
#else
#define STATIC
#endif

#ifdef _WIN32
#define strcasecmp      stricmp
#endif


#ifdef _WIN32
#define HOME_CONFIG_FILE "qstat.cfg"
#else
#define HOME_CONFIG_FILE ".qstatrc"
#endif


static server_type ** config_types;
static int n_config_types;
static int max_config_types;
static int next_gametype_id= LAST_BUILTIN_SERVER+1;

static int load_config_file( char *filename);
static int try_load_config_file( char *filename, int show_error);
static int pf_top_level( char *text, void *context);
static int pf_gametype_new( char *text, void *context);
static int pf_gametype_modify( char *text, void *context);

static int set_game_type_value( server_type *gametype, int key, char *value);
static int modify_game_type_value( server_type *gametype, int key, char *value);
static int set_packet_value( server_type *gametype, char *value,
	char *packet_name, char **packet, int *len);

static char *first_token( char *text);
static char *next_token();
static char *next_token_dup();
static char *next_value();
static char * parse_value( char *source, int len);
static unsigned int parse_hex( char *hex, int len, int *error);
static unsigned int parse_oct( char *oct, int len, int *error);
static char *force_lower_case( char *string);
static char *force_upper_case( char *string);
static char *get_token();
static void * memdup( void *mem, unsigned int len);

int parse_config_file( FILE *file);

static int (*parse_func)( char *, void *);
static void *parse_context;
static int line;
static char *current_file_name;

static char *parse_text;
static char *parse_end;
static char *lex;
static char *token_end;
static char token_buf[1024];
static int value_len;

static int debug= 0;

#define REPORT_ERROR(a)		print_location(); fprintf a; fprintf(stderr, "\n")

enum {
    CK_NONE = 0,
    CK_MASTER_PROTOCOL,
    CK_MASTER_QUERY,
    CK_MASTER_PACKET,
    CK_FLAGS,
    CK_NAME,
    CK_DEFAULT_PORT,
    CK_GAME_RULE,
    CK_TEMPLATE_VAR,
    CK_MASTER_TYPE,
    CK_STATUS_PACKET,
    CK_STATUS2_PACKET,
    CK_PLAYER_PACKET,
    CK_RULE_PACKET,
    CK_PORT_OFFSET
};

typedef struct _config_key {
    int key;
    char *key_name;
} ConfigKey;

static ConfigKey new_keys[] = {
{ CK_MASTER_PROTOCOL, "master protocol" },
{ CK_MASTER_QUERY, "master query" },
{ CK_MASTER_PACKET, "master packet" },
{ CK_FLAGS, "flags" },
{ CK_NAME, "name" },
{ CK_DEFAULT_PORT, "default port" },
{ CK_GAME_RULE, "game rule" },
{ CK_TEMPLATE_VAR, "template var" },
{ CK_MASTER_TYPE, "master for gametype" },
{ CK_STATUS_PACKET, "status packet" },
{ CK_STATUS2_PACKET, "status2 packet" },
{ CK_PLAYER_PACKET, "player packet" },
{ CK_RULE_PACKET, "rule packet" },
{ CK_PORT_OFFSET, "status port offset" },
{ 0, NULL },
};

static ConfigKey modify_keys[] = {
{ CK_MASTER_PROTOCOL, "master protocol" },
{ CK_MASTER_QUERY, "master query" },
{ CK_MASTER_PACKET, "master packet" },
{ CK_FLAGS, "flags" },
{ CK_MASTER_TYPE, "master for gametype" },
{ 0, NULL },
};

typedef struct {
    const char *name;
    int value;
} ServerFlag;

#define SERVER_FLAG(x) { #x, x }
ServerFlag server_flags[] = {
    SERVER_FLAG(TF_SINGLE_QUERY),
    SERVER_FLAG(TF_OUTFILE),
    SERVER_FLAG(TF_MASTER_MULTI_RESPONSE),
    SERVER_FLAG(TF_TCP_CONNECT),
    SERVER_FLAG(TF_QUERY_ARG),
    SERVER_FLAG(TF_QUERY_ARG_REQUIRED),
    SERVER_FLAG(TF_QUAKE3_NAMES),
    SERVER_FLAG(TF_TRIBES2_NAMES),
    SERVER_FLAG(TF_SOF_NAMES),
    SERVER_FLAG(TF_U2_NAMES),
    SERVER_FLAG(TF_RAW_STYLE_QUAKE),
    SERVER_FLAG(TF_RAW_STYLE_TRIBES),
    SERVER_FLAG(TF_RAW_STYLE_GHOSTRECON),
    SERVER_FLAG(TF_NO_PORT_OFFSET),
    SERVER_FLAG(TF_SHOW_GAME_PORT),
    { NULL, 0 }
};
#undef SERVER_FLAG

static int get_config_key( char *first_token, ConfigKey *keys);
static void add_config_type( server_type *gametype);
static server_type * get_config_type( char *game_type);
static void copy_server_type( server_type *dest, server_type *source);
static server_type * get_server_type( char *game_type);
static server_type * get_builtin_type( char *game_type);

typedef struct _gametype_context {
    char *type;
    char *extend_type;
    server_type *gametype;
} GameTypeContext;


static void
print_location()
{
    fprintf( stderr, "%s line %d: ", current_file_name, line);
}

int
qsc_load_default_config_files()
{
    int rc= 0;
    char *filename= NULL, *var;
    char path[1024];

    var= getenv( "QSTAT_CONFIG");
    if ( var != NULL && var[0] != '\0')  {
	rc= try_load_config_file( var, 1);
	if ( rc == 0 || rc == -1)
	    return rc;
    }

    var= getenv( "HOME");
    if ( var != NULL && var[0] != '\0')  {
	int len= strlen(var);
	if ( len > 900)
	    len= 900;
	strncpy( path, var, len);
	path[len]= '\0';
	strcat( path, "/");
	strcat( path, HOME_CONFIG_FILE);
/*	sprintf( path, "%s/%s", var, HOME_CONFIG_FILE); */
	rc= try_load_config_file( path, 0);
	if ( rc == 0 || rc == -1)
	    return rc;
    }

#ifdef sysconfdir
    strcpy( path, sysconfdir "/qstat.cfg");
    filename= path;
#elif defined(_WIN32)
    if ( filename == NULL && _pgmptr && strchr( _pgmptr, '\\'))  {
	    char *slash= strrchr( _pgmptr, '\\');
	    strncpy( path, _pgmptr, slash - _pgmptr);
	    path[slash - _pgmptr]= '\0';
	    strcat( path, "\\qstat.cfg");
	    filename= path;
    }
#endif

    if ( filename != NULL)  {
	rc= try_load_config_file( filename, 0);
	if ( rc == 0 || rc == -1)
	    return rc;
    }
    return rc;
/*
    if ( rc == -2 && show_error)  {
	perror( filename);
	fprintf( stderr, "Error: Could not open config file \"%s\"\n",
		filename);
    }
    else if ( rc == -1 && show_error)
	fprintf( stderr, "Error: Error loading $QSTAT_CONFIG file\n");
    return rc;
*/

#ifdef foo
    filename= getenv( "HOME");
    if ( filename != NULL && filename[0] != '\0')  {
	char path[1024];
	sprintf( path, "%s/%s", filename, HOME_CONFIG_FILE);
    }
/* 1. $QSTAT_CONFIG
   2. UNIX: $HOME/.qstatrc         WIN: $HOME/qstat.cfg
   3. UNIX: sysconfdir/qstat.cfg   WIN: qstat.exe-dir/qstat.cfg
*/

    rc= load_config_file( "qstat.cfg");
    if ( rc == -1)
	fprintf( stderr, "Warning: Error loading default qstat.cfg\n");
    return 0;
#endif
}

int
qsc_load_config_file( char *filename)
{
    int rc= load_config_file( filename);
    if ( rc == -2)  {
	perror( filename);
	fprintf( stderr, "Could not open config file \"%s\"\n",
		filename);
	return -1;
    }
    return rc;
}

server_type **
qsc_get_config_server_types( int *n_config_types_ref)
{
    if ( n_config_types_ref)
	*n_config_types_ref= n_config_types;
    return config_types;
}

STATIC int
try_load_config_file( char *filename, int show_error)
{
    int rc= load_config_file( filename);
    if ( rc == -2 && show_error)  {
	perror( filename);
	fprintf( stderr, "Error: Could not open config file \"%s\"\n",
		filename);
    }
    else if ( rc == -1)
	fprintf( stderr, "Error: Could not load config file \"%s\"\n",
		filename);
    return rc;
}

STATIC int
load_config_file( char *filename)
{
    FILE *file;
    int rc;

    file= fopen( filename, "r");
    if ( file == NULL)
	return -2;

    current_file_name= filename;
    rc= parse_config_file( file);
    fclose(file);
    return rc;
}

int
parse_config_file( FILE *file)
{
    char buf[4096];
    int rc;

    line= 0;
    parse_func= pf_top_level;

    while ( fgets( buf, sizeof(buf), file) != NULL)  {
	int len= strlen(buf);
	while ( len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
	   len--;
	buf[len]= '\0';
	line++;
	rc= parse_func( buf, parse_context);
	if ( rc != 0)
	    return rc;
    }
    return 0;
}


/* Top level
 *   Keywords: gametype
 */
STATIC int
pf_top_level( char *text, void *_context)
{
    GameTypeContext *context;
    server_type *extend;
    char *token, *game_type, *extend_type;
    token= first_token( text);
    if ( token == NULL)
	return 0;

    if ( strcmp( token, "gametype") != 0)  {
	REPORT_ERROR(( stderr, "Unknown config command \"%s\"", token));
	return -1;
    }

    game_type= next_token_dup();
    if ( game_type == NULL)  {
	REPORT_ERROR(( stderr, "Missing game type"));
	return -1;
    }

    force_lower_case( game_type);

    token= next_token();
    if ( token == NULL)  {
	REPORT_ERROR(( stderr, "Expecting \"new\" or \"modify\""));
	return -1;
    }

    if ( strcmp( token, "new") == 0)  {
	parse_func= pf_gametype_new;
    }
    else if ( strcmp( token, "modify") == 0)  {
	parse_func= pf_gametype_modify;
    }
    else  {
	REPORT_ERROR(( stderr, "Expecting \"new\" or \"modify\""));
	return -1;
    }

    context= (GameTypeContext*) malloc( sizeof(GameTypeContext));
    context->type= game_type;
    context->extend_type= NULL;
    context->gametype= NULL;

    token= next_token();

    if ( parse_func == pf_gametype_modify)  {
	if ( token != NULL)  {
	    REPORT_ERROR(( stderr, "Extra text after gametype modify"));
	    return -1;
	}
	context->gametype= get_server_type( game_type);
	if ( context->gametype == NULL)  {
	    REPORT_ERROR(( stderr, "Unknown game type \"%s\"", game_type));
	    free(context);
	    return -1;
	}
	parse_context= context;
	return 0;
    }

    if ( token == NULL || strcmp( token, "extend") != 0)  {
	REPORT_ERROR(( stderr, "Expecting \"extend\""));
	return -1;
    }

    extend_type= next_token();

    if ( extend_type == NULL)  {
	REPORT_ERROR(( stderr, "Missing extend game type"));
	return -1;
    }

    force_lower_case( extend_type);

    if ( strcasecmp( extend_type, game_type) == 0)  {
	REPORT_ERROR(( stderr, "Identical game type and extend type"));
	return -1;
    }

    context->extend_type= extend_type;
    extend= get_server_type( extend_type);
    if ( extend == NULL)  {
	REPORT_ERROR(( stderr, "Unknown extend game type \"%s\"", extend_type));
	return -1;
    }

    /* Over-write a previous gametype new */
    context->gametype= get_config_type( game_type);
    if ( context->gametype == NULL)
	context->gametype= (server_type*) malloc( sizeof(server_type));
    copy_server_type( context->gametype, extend);

    /* Set flag for new type-id if not re-defining previous config type */
    if ( get_config_type( game_type) == NULL)
	context->gametype->id= 0;

    context->gametype->type_string= game_type;
    context->gametype->type_prefix= strdup(game_type);
    force_upper_case( context->gametype->type_prefix);
    context->gametype->type_option= (char*)malloc( strlen(game_type)+2);
    context->gametype->type_option[0]= '-';
    strcpy( &context->gametype->type_option[1], game_type);

    parse_context= context;

    return 0;
}

STATIC int
pf_gametype_modify( char *text, void *_context)
{
    GameTypeContext *context= (GameTypeContext*) _context;
    char *token;
    int key;

    token= first_token( text);
    if ( token == NULL)
	return 0;

    if ( strcmp( token, "end") == 0)  {
	parse_func= pf_top_level;
	parse_context= NULL;
	return 0;
    }

    key= get_config_key( token, modify_keys);

    token= next_token();

    if ( strcmp( token, "=") != 0)  {
	REPORT_ERROR(( stderr, "Expecting \"=\", found \"%s\"", token));
	return -1;
    }

    token= next_value();
    if ( token == NULL)  {
	REPORT_ERROR(( stderr, "Missing value after \"=\""));
	return -1;
    }

    if ( debug)
	printf( "%d %s = <%s>\n", key, modify_keys[key-1].key_name,
		token?token:"");

    return modify_game_type_value( context->gametype, key, token);
}

STATIC int
pf_gametype_new( char *text, void *_context)
{
    GameTypeContext *context= (GameTypeContext*) _context;
    char *token;
    int key;

    token= first_token( text);
    if ( token == NULL)
	return 0;

    if ( strcmp( token, "end") == 0)  {
	add_config_type( context->gametype);
	parse_func= pf_top_level;
	parse_context= NULL;
	return 0;
    }

    key= get_config_key( token, new_keys);

    if ( key <= 0)
	return key;

    token= next_token();
    if ( strcmp( token, "=") != 0)  {
	REPORT_ERROR(( stderr, "Expecting \"=\", found \"%s\"", token));
	return -1;
    }

    token= next_value();
    if ( token == NULL && (key != CK_MASTER_PROTOCOL && key != CK_MASTER_QUERY))  {
	REPORT_ERROR(( stderr, "Missing value after \"=\""));
	return -1;
    }

    if ( debug)
	printf("%d %s = <%s>\n", key, new_keys[key-1].key_name, token?token:"");

    return set_game_type_value( context->gametype, key, token);
}

STATIC int
get_config_key( char *first_token, ConfigKey *keys)
{
    char key_name[1024], *token;
    int key= 0;

    strcpy( key_name, first_token);
    do  {
	int k;
	for ( k= 0; keys[k].key_name; k++)  {
	    if ( strcmp( keys[k].key_name, key_name) == 0)  {
		key= keys[k].key;
		break;
	    }
	}
	if ( key)
	    break;
	token= next_token();
	if ( token == NULL || strcmp( token, "=") == 0)
	    break;
	if ( strlen(key_name) + strlen(token) > sizeof(key_name)-2)  {
	    REPORT_ERROR(( stderr, "Key name too long"));
	    return -1;
	}
	strcat( key_name, " ");
	strcat( key_name, token);
    } while ( 1);

    if ( key == 0)  {
	REPORT_ERROR(( stderr, "Unknown config key \"%s\"", key_name));
	return -1;
    }

    return key;
}

STATIC int get_server_flag_value(const char* value, unsigned len)
{
    int i= 0;

    for ( i= 0; server_flags[i].name; ++i)  {
	if ( len == strlen(server_flags[i].name) &&
		strncmp( server_flags[i].name, value, len) == 0)  {
	    return server_flags[i].value;
	}
    }

    return -1;
}

STATIC int parse_server_flags(const char* value)
{
    int val = 0, v, first = 1;
    const char* s = value;
    const char* e;

    while(*s)
    {
	while(isspace((unsigned char)*s))
	    ++s;

	if(!first)
	{
	    if(*s != '|') {
		REPORT_ERROR(( stderr, "Syntax error: expecting |"));
		val = -1;
		break;
	    }

	    ++s;

	    while(isspace((unsigned char)*s))
		++s;
	}
	else
	    first = 0;

	e = s;

	while(isalnum((unsigned char)*e) || *e == '_')
	    ++e;

	if(e == s) {
	    REPORT_ERROR(( stderr, "Syntax error: expecting flag"));
	    val = -1;
	    break;
	}

	v = get_server_flag_value(s, e-s);
	if(v == -1) {
	    REPORT_ERROR(( stderr, "Syntax error: invalid flag"));
	    val = -1;
	    break;
	}

	s = e;
	val |= v;
    }

    return val;
}

STATIC int
set_game_type_value( server_type *gametype, int key, char *value)
{
    switch ( key)  {
    case CK_NAME:
	gametype->game_name= strdup(value);
	break;
    case CK_FLAGS:
	{
	    int flags = parse_server_flags(value);
	    if(flags == -1)
		return -1;
	    gametype->flags = flags;
	}
	break;
    case CK_DEFAULT_PORT: {
	unsigned short port;
	if ( sscanf( value, "%hu", &port) != 1)  {
	    REPORT_ERROR(( stderr, "Syntax error on port.  Should be a number between 1 and 65535."));
	    return -1;
	}
	gametype->default_port= port;
	break;
	}
    case CK_GAME_RULE:
	gametype->game_rule= strdup(value);
	break;
    case CK_TEMPLATE_VAR:
	gametype->template_var= strdup(value);
	force_upper_case( gametype->template_var);
	break;
    case CK_MASTER_PROTOCOL:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master protocol on non-master game type\n"));
	    return -1;
	}
	if ( value)
	    gametype->master_protocol= strdup(value);
	else
	    gametype->master_protocol= NULL;
	break;
    case CK_MASTER_QUERY:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master query on non-master game type\n"));
	    return -1;
	}
	if ( value)
	    gametype->master_query= strdup(value);
	else
	    gametype->master_query= NULL;
	break;
    case CK_MASTER_TYPE:
	{
	server_type *type;
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master type on non-master game type\n"));
	    return -1;
	}
	force_lower_case( value);
	type= get_server_type( value);
	if ( type == NULL)  {
	    REPORT_ERROR((stderr, "Unknown server type \"%s\"\n", value));
	    return -1;
	}
	gametype->master= type->id;
	}
	break;
    case CK_MASTER_PACKET:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master packet on non-master game type"));
	    return -1;
	}
	if ( value == NULL || value[0] == '\0')  {
	    REPORT_ERROR((stderr, "Empty master packet"));
	    return -1;
	}
	gametype->master_packet= memdup( value, value_len);
	gametype->master_len= value_len;
	break;
    case CK_STATUS_PACKET:
	return set_packet_value( gametype, value, "status",
		&gametype->status_packet, &gametype->status_len);
    case CK_STATUS2_PACKET:
	return set_packet_value( gametype, value, "status2",
		&gametype->rule_packet, &gametype->rule_len);
    case CK_PLAYER_PACKET:
	return set_packet_value( gametype, value, "player",
		&gametype->player_packet, &gametype->player_len);
    case CK_RULE_PACKET:
	return set_packet_value( gametype, value, "rule",
		&gametype->rule_packet, &gametype->rule_len);
    case CK_PORT_OFFSET:  {
	short port;
	if ( sscanf( value, "%hd", &port) != 1)  {
	    REPORT_ERROR(( stderr, "Syntax error on port.  Should be a number between 32767 and -32768."));
	    return -1;
	}
	gametype->port_offset= port;
	break;
	}
    }

    return 0;
}

STATIC int
set_packet_value( server_type *gametype, char *value, char *packet_name,
	char **packet, int *len)
{
    if ( gametype->master)  {
	REPORT_ERROR((stderr, "Cannot set info packet on master game type"));
	return -1;
    }
	if ( value == NULL || value[0] == '\0')  {
	    REPORT_ERROR((stderr, "Empty %s packet", packet_name));
	    return -1;
	}
	if ( *packet == NULL)  {
	    REPORT_ERROR((stderr, "Cannot set %s packet; extend game type does not define a %s packet", packet_name, packet_name));
	    return -1;
	}
	*packet= memdup( value, value_len);
	*len= value_len;
    return 0;
}

STATIC int
modify_game_type_value( server_type *gametype, int key, char *value)
{
    switch ( key)  {
    case CK_FLAGS:
	{
	    int flags = parse_server_flags(value);
	    if(flags == -1)
		return -1;
	    gametype->flags = flags;
	}
	break;
    case CK_MASTER_PROTOCOL:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master protocol on non-master game type"));
	    return -1;
	}
	if ( value)
	    gametype->master_protocol= strdup(value);
	else
	    gametype->master_protocol= NULL;
	break;
    case CK_MASTER_QUERY:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master query on non-master game type"));
	    return -1;
	}
	if ( value)
	    gametype->master_query= strdup(value);
	else
	    gametype->master_query= NULL;
	break;
    case CK_MASTER_PACKET:
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master packet on non-master game type"));
	    return -1;
	}
	if ( value == NULL || value[0] == '\0')  {
	    REPORT_ERROR((stderr, "Empty master packet"));
	    return -1;
	}
	gametype->master_packet= memdup( value, value_len);
	gametype->master_len= value_len;
	break;
    case CK_MASTER_TYPE:
	{
	server_type *type;
	if ( ! gametype->master)  {
	    REPORT_ERROR((stderr, "Cannot set master type on non-master game type\n"));
	    return -1;
	}
	force_lower_case( value);
	type= get_server_type( value);
	if ( type == NULL)  {
	    REPORT_ERROR((stderr, "Unknown server type \"%s\"\n", value));
	    return -1;
	}
	gametype->master= type->id;
	}
	break;
    }
    return 0;
}

STATIC server_type *
get_server_type( char *game_type)
{
    server_type *result;
    result= get_builtin_type( game_type);
    if ( result != NULL)
	return result;

    return get_config_type( game_type);
}

STATIC server_type*
get_builtin_type( char* type_string)
{
    server_type *type= &builtin_types[0];

    for ( ; type->id != Q_UNKNOWN_TYPE; type++)
	if ( strcmp( type->type_string, type_string) == 0)
	    return type;
    return NULL;
}

STATIC void
add_config_type( server_type *gametype)
{
    if ( gametype->id == 0)  {
	if ( next_gametype_id >= MASTER_SERVER)  {
	    REPORT_ERROR(( stderr, "Exceeded game type limit, ignoring \"%s\"",
		gametype->type_string));
	    return;
	}
	gametype->id= next_gametype_id;
	if ( gametype->master)
	    gametype->id |= MASTER_SERVER;
	next_gametype_id++;
    }

    if ( max_config_types == 0)  {
	max_config_types= 4;
	config_types= (server_type**) malloc(sizeof(server_type*) * (max_config_types + 1));
    }
    else if ( n_config_types >= max_config_types)  {
	max_config_types*= 2;
	config_types= (server_type**) realloc( config_types,
		sizeof(server_type*) * (max_config_types + 1));
    }
    config_types[ n_config_types]= gametype;
    n_config_types++;
}

STATIC server_type *
get_config_type( char *game_type)
{
    int i;
    for ( i= 0; i < n_config_types; i++)
	if ( strcmp( config_types[i]->type_string, game_type) == 0)
	    return config_types[i];

    return NULL;
}

STATIC void
copy_server_type( server_type *dest, server_type *source)
{
    *dest= *source;
}

STATIC void *
memdup( void *mem, unsigned int len)
{
    void *result= malloc( len);
    memcpy( result, mem, len);
    return result;
}

/* Parsing primitives
 */

STATIC char *
first_token( char *text)
{
    parse_text= text;
    parse_end= parse_text + strlen( parse_text);
    lex= text;
    token_end= text;

    if ( *lex == '#')
	return NULL;

    return get_token();
}

STATIC char *
next_token()
{
    lex= token_end;
    return get_token();
}

STATIC char *
next_token_dup()
{
    return strdup( next_token());
}

STATIC char *
get_token()
{
    char *token= &token_buf[0];
    while ( isspace((unsigned char)*token_end))
	token_end++;

    if ( token_end == parse_end)
	return NULL;

    while ( (isalnum((unsigned char)*token_end) || *token_end == '.' || *token_end == '_' || *token_end == '-')
		&& token < &token_buf[0] + sizeof(token_buf))
	*token++= *token_end++;

    if ( token == &token_buf[0])
	*token++= *token_end++;
    *token= '\0';
    return &token_buf[0];
}

extern void print_packet( struct qserver *server, char *buf, int buflen);

STATIC char *
next_value()
{
    char *token= &token_buf[0];
    while ( isspace((unsigned char)*token_end))
	token_end++;

    if ( token_end == parse_end)
	return NULL;

    while ( token_end < parse_end && token < &token_buf[0] + sizeof(token_buf))
	*token++= *token_end++;

    *token--= '\0';

    if ( strchr( token_buf, '\\'))
	return parse_value(token_buf, (token - token_buf)+1);

    while ( isspace((unsigned char)*token))
	*token--= '\0';

    value_len= token - token_buf + 1;

    return &token_buf[0];
}

STATIC char *
parse_value( char *source, int len)
{
    char *value, *v, *end;
    int error= 0;

    value= (char*)malloc( len + 1);
    end = v = value;

/*
printf( "parse_value <%.*s>\n", len, source);
*/

    for ( ; len; len--, source++)  {
	if ( *source != '\\')  {
	    *v++= *source;
	    if ( *source != ' ')
		end= v;
	    continue;
	}
	source++; len--;
	if ( len == 0)
	    break;
	if ( *source == '\\')
	    *v++= *source;
	else if ( *source == 'n')
	    *v++= '\n';
	else if ( *source == 'r')
	    *v++= '\r';
	else if ( *source == ' ')
	    *v++= ' ';
	else if ( *source == 'x')  {
	    source++; len--;
	    if ( len < 2)
		break;
	    *v++= parse_hex(source, 2, &error);
	    if ( error)
		break;
	    source++; len--;
	}
	else if ( isdigit( (unsigned char)*source))  {
	    if ( len < 3)
		break;
	    *v++= parse_oct(source, 3, &error);
	    if ( error)
		break;
	    source++; len--;
	    source++; len--;
	}
	else  {
	    error= 1;
	    REPORT_ERROR(( stderr, "Invalid character escape \"%.*s\"", 2,
		source-1));
	    break;
	}
	end= v;
    }

    if ( error)  {
	free(value);
	return NULL;
    }

    value_len= end - value;
    memcpy( token_buf, value, value_len);
    token_buf[value_len]= '\0';

/*
print_packet(NULL, token_buf, value_len);
*/

    free(value);
    return &token_buf[0];
}

STATIC unsigned int
parse_hex( char *hex, int len, int *error)
{
    char *save_hex= hex;
    int save_len= len;
    unsigned int result= 0;
    *error= 0;
    for ( ; len; len--, hex++)  {
	result <<= 4;
	if ( ! isxdigit( (unsigned char)*hex))  {
	    *error= 1;
	    REPORT_ERROR(( stderr, "Invalid hex \"%.*s\"", save_len+2,
		save_hex-2));
	    return 0;
	}
	if ( *hex >= '0' && *hex <= '9')
	    result|= *hex - '0';
	else if ( *hex >= 'A' && *hex <= 'F')
	    result|= ((*hex - 'A') + 10);
	else if ( *hex >= 'a' && *hex <= 'f')
	    result|= ((*hex - 'a') + 10);
    }
    return result;
}

STATIC unsigned int
parse_oct( char *oct, int len, int *error)
{
    char *save_oct= oct;
    int save_len= len;
    unsigned int result= 0;
    *error= 0;
    for ( ; len; len--, oct++)  {
	result <<= 3;
	if ( *oct >= '0' && *oct <= '7')
	    result|= *oct - '0';
	else  {
	    *error= 1;
	    REPORT_ERROR(( stderr, "Invalid octal \"%.*s\"", save_len+1,
		save_oct-1));
	    return 0;
	}
    }
    return result;
}

STATIC char *
force_lower_case( char *string)
{
    char *s= string;
    for ( ; *s; s++)
	*s= tolower(*s);
    return string;
}

STATIC char *
force_upper_case( char *string)
{
    char *s= string;
    for ( ; *s; s++)
	*s= toupper(*s);
    return string;
}

