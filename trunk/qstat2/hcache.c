/*
 * qstat 2.4
 * by Steve Jankowski
 * steve@activesw.com
 * http://www.activesw.com/people/steve/qstat.html
 *
 * Thanks to Per Hammer for the OS/2 patches (per@mindbend.demon.co.uk)
 * Thanks to John Ross Hunt for the OpenVMS Alpha patches (bigboote@ais.net)
 * Thanks to Scott MacFiggen for the quicksort code (smf@activesw.com)
 *
 * Inspired by QuakePing by Len Norton
 *
 * Copyright 1996,1997,1998,1999 by Steve Jankowski
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "qstat.h"

#ifdef _ISUNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#endif

#ifdef __hpux
#define STATIC static
#else
#define STATIC
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ~0
#endif


typedef struct _cache_entry {
    unsigned long ipaddr;
    char *hostname[5];
} cache_entry;

static cache_entry *hcache;
static int n_entry;
static int max_entry;
static char *last_filename;
static int n_changes;
static int debug;

static void write_file(FILE *file);
static cache_entry * init_entry( unsigned long ipaddr, char *hostname,
	cache_entry *known);
static cache_entry * find_entry( unsigned long ipaddr);
static void free_entry(  cache_entry *entry);
static cache_entry * validate_entry( cache_entry *entry);
static cache_entry * find_host_entry( char *hostname);
static void add_hostname( cache_entry *entry, const char *hostname);

int
hcache_open( char *filename, int update)
{
    FILE *file;
    char line[500], ipstr[500], hostname[500];
    char *l;
    int line_no, end;
    unsigned long ip1, ip2, ip3, ip4, ipaddr;
    cache_entry *entry;

    file= fopen( filename, update?"r+":"r");
    if ( file == NULL)  {
	if ( errno == ENOENT)  {
	    fprintf( stderr, "Creating new host cache \"%s\"\n", filename);
	    last_filename= filename;
	    return 0;
	}
	perror( filename);
	return -1;
    }
    last_filename= filename;

    for ( line_no= 1; fgets( line, sizeof(line), file) != NULL; line_no++)  {
	if ( strlen(line) < 2)
	    continue;
	if ( line[strlen(line)-1] != '\n')  {
	    printf( "%d: line too long\n", line_no);
	    continue;
	}
	l= line;
	while ( isspace( *l)) l++;
	if ( *l == '#' || *l == '\0')
	    continue;
	if ( sscanf( l, "%s%n", ipstr, &end) != 1)  {
	    printf( "%d: parse error\n", line_no);
	    continue;
	}
	if ( sscanf( ipstr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4) != 4)  {
	    init_entry( 0, ipstr, NULL);
	    continue;
	}

	if ( (ip1&0xffffff00) || (ip2&0xffffff00) ||
		(ip3&0xffffff00) || (ip4&0xffffff00))  {
	    printf( "%d: invalid IP address \"%s\"\n", line_no, ipstr);
	    continue;
	}
 	ipaddr= (ip1<<24) | (ip2<<16) | (ip3<<8) | ip4;

	entry= init_entry( ipaddr, NULL, NULL);
	while ( 1)  {
	    l+= end;
	    while ( isspace( *l)) l++;
	    if ( *l == '#' || *l == '\0')
		break;
	    hostname[0]= '\0';
	    if ( sscanf( l, "%s%n", hostname, &end) != 1)  {
		printf( "%d: parse error\n", line_no);
		continue;
	    }
	    init_entry( ipaddr, hostname, entry);
	}
    }
    fclose(file);
    return 0;
}

STATIC cache_entry *
init_entry( unsigned long ipaddr, char *hostname, cache_entry *known)
{
    cache_entry *entry;
    int e= 0, h;
    if ( n_entry == max_entry)  {
	if ( max_entry == 0)  {
	    max_entry= 50;
	    hcache= (cache_entry*) malloc(sizeof(cache_entry) * max_entry * 2);
	}
	else  {
	    hcache= (cache_entry*) realloc( hcache,
		sizeof(cache_entry) * max_entry * 2);
	}
	memset( hcache+n_entry, 0, sizeof(cache_entry) * max_entry *
		(n_entry==0?2:1));
	max_entry*= 2;
    }

    if ( ipaddr == 0)  {
	entry= find_host_entry( hostname);
	if ( entry == NULL)  {
	    hcache[n_entry].hostname[0]= strdup( hostname);
	    return &hcache[n_entry++];
	}
	return entry;
    }

    if ( known != NULL)
	entry= known;
    else  {
	for ( e= 0; e < n_entry; e++)
	    if ( hcache[e].ipaddr == ipaddr)
	        break;
	entry= &hcache[e];
	entry->ipaddr= ipaddr;
    }

    if ( hostname && hostname[0] != '\0')  {
	for ( h= 0; h < 5; h++)
	    if ( entry->hostname[h] == NULL)  {
		entry->hostname[h]= strdup( hostname);
		break;
	    }
    }
    if ( e == n_entry)
	n_entry++;
    return entry;
}

STATIC cache_entry *
find_host_entry( char *hostname)
{
    cache_entry *entry= &hcache[0];
    char **ehost;
    int e, h;
    char first= *hostname;
    for ( e= 0; e < n_entry; e++, entry++)  {
	ehost= &entry->hostname[0];
	for ( h= 0; h < 5; h++, ehost++)
	    if ( *ehost && first == **ehost && strcmp( hostname, *ehost) == 0)
		return entry;
    }
    return NULL;
}

void
hcache_write_file( char *filename)
{
    FILE *file;
    if ( filename != NULL)
	file= fopen( filename, "w");
    else
	file= stdout;
    if ( file == NULL)  {
	perror( filename);
	return;
    }
    write_file( file);
}

void
hcache_update_file()
{
    FILE *file;
    if ( last_filename == NULL || n_changes == 0)
	return;

    file= fopen( last_filename, "w");
    if ( file == NULL)  {
	perror( last_filename);
	return;
    }
    write_file( file);
}

STATIC void
write_file( FILE *file)
{
    int e, h;
    for ( e= 0; e < n_entry; e++)  {
	unsigned long ipaddr= hcache[e].ipaddr;
	if ( ipaddr == 0)
	    continue;
	fprintf( file, "%lu.%lu.%lu.%lu", (ipaddr&0xff000000)>>24,
		(ipaddr&0xff0000)>>16, (ipaddr&0xff00)>>8, ipaddr&0xff);
	if ( hcache[e].hostname[0])  {
	    for ( h= 0; h < 5; h++)
		if ( hcache[e].hostname[h] != NULL)
		    fprintf( file, "%c%s", h?' ':'\t', hcache[e].hostname[h]);
	}
	fprintf( file, "\n");
    }
    fclose( file);
}

void
hcache_invalidate()
{
    int e;
    for ( e= 0; e < n_entry; e++)
	if ( hcache[e].ipaddr != 0)
	    memset( & hcache[e].hostname[0], 0, sizeof( hcache[e].hostname));
}

void
hcache_validate()
{
    int e;
    char **alias;
    struct hostent *ent;
    unsigned long ipaddr;
    cache_entry *entry;

    for ( e= 0; e < n_entry; e++)  {
	fprintf( stderr, "\r%d / %d  validating ", e, n_entry);
	if ( hcache[e].ipaddr != 0)  {
	    ipaddr= hcache[e].ipaddr;
	    fprintf( stderr, "%lu.%lu.%lu.%lu", (ipaddr&0xff000000)>>24,
		(ipaddr&0xff0000)>>16, (ipaddr&0xff00)>>8, ipaddr&0xff);
	    ipaddr= htonl( ipaddr);
	    ent= gethostbyaddr( (char*)&ipaddr, sizeof(unsigned long),
		AF_INET);
	}
	else if ( hcache[e].hostname[0] != NULL)  {
	    fprintf( stderr, "%s", hcache[e].hostname[0]);
	    ent= gethostbyname( hcache[e].hostname[0]);
	    if ( ent != NULL)  {
		memcpy( &ipaddr, ent->h_addr_list[0], sizeof(ipaddr));
		ipaddr= ntohl( ipaddr);
		if ( (entry= find_entry( ipaddr)) != NULL)  {
		    add_hostname( entry, hcache[e].hostname[0]);
		    free_entry( &hcache[e]);
		}
		else
		    hcache[e].ipaddr= ipaddr;
	    }
	}
	else
	    continue;
	if ( ent == NULL)
	    continue;

	if ( ent->h_name && ent->h_name[0] != '\0')
	    add_hostname( &hcache[e], ent->h_name);
	printf( "h_name %s\n", ent->h_name?ent->h_name:"NULL");
	alias= ent->h_aliases;
	while ( *alias)  {
	    add_hostname( &hcache[e], *alias);
	    printf( "h_aliases %s\n", *alias);
	    alias++;
	}
    }
}

STATIC cache_entry *
validate_entry( cache_entry *entry)
{
    struct hostent *ent;
    char **alias;
    cache_entry *tmp;
    unsigned long ipaddr;

    if ( entry->ipaddr != 0)  {
	ipaddr= htonl( entry->ipaddr);
/*	    fprintf( stderr, "%u.%u.%u.%u", (ipaddr&0xff000000)>>24,
		(ipaddr&0xff0000)>>16, (ipaddr&0xff00)>>8, ipaddr&0xff);
*/
	ent= gethostbyaddr( (char*)&ipaddr, sizeof(unsigned long), AF_INET);
    }
    else if ( entry->hostname[0] != NULL)  {
/* 	    fprintf( stderr, "%s", entry->hostname[0]);
*/
	ent= gethostbyname( entry->hostname[0]);
	if ( ent != NULL)  {
	    memcpy( &ipaddr, ent->h_addr_list[0], sizeof(ipaddr));
	    ipaddr= ntohl( ipaddr);
	    if ( (tmp= find_entry( ipaddr)) != NULL)  {
		add_hostname( tmp, entry->hostname[0]);
		free_entry( entry);
		entry= tmp;
	    }
	    else
		entry->ipaddr= ipaddr;
	}
    }
    else
	return NULL;

    if ( ent == NULL)
	return NULL;

    if ( ent->h_name && ent->h_name[0] != '\0')
	add_hostname( entry, ent->h_name);
    alias= ent->h_aliases;
    while ( *alias)  {
	add_hostname( entry, *alias);
	alias++;
    }
    return entry;
}

unsigned long
hcache_lookup_hostname( char *hostname)
{
    cache_entry *entry;
    int e, h;
if ( debug) printf( "looking up %s\n", hostname);
    for ( e= 0; e < n_entry; e++)  {
	for ( h= 0; h < 5; h++)  {
	    if ( hcache[e].hostname[h] &&
			strcmp( hostname, hcache[e].hostname[h]) == 0)
		return hcache[e].ipaddr;
	}
    }
    entry= init_entry( 0, hostname, NULL);
    if ( entry->ipaddr == 0)  {
if ( debug) printf( "validating %s\n", hostname);
	entry= validate_entry( entry);
	n_changes++;
    }
if ( debug) printf( "returning %lx\n", entry->ipaddr);
    if ( entry != NULL && entry->ipaddr)
	return entry->ipaddr;
    return INADDR_NONE;
}

char *
hcache_lookup_ipaddr( unsigned long ipaddr)
{
    cache_entry *entry;
    int e;
    for ( e= 0; e < n_entry; e++)
	if ( hcache[e].ipaddr == ipaddr)
	    return hcache[e].hostname[0];
    entry= init_entry( ipaddr, 0, NULL);
if ( debug) printf( "validating %lx\n", ipaddr);
    validate_entry( entry);
    n_changes++;
    return entry ? entry->hostname[0] : NULL;
}

STATIC cache_entry *
find_entry( unsigned long ipaddr)
{
    int e;
    for ( e= 0; e < n_entry; e++)
	if ( hcache[e].ipaddr == ipaddr)
	    return &hcache[e];
    return NULL;
}

STATIC void
free_entry(  cache_entry *entry)
{
    int h;
    for ( h= 0; h < 5; h++)
	if ( entry->hostname[h] != NULL)
	    free( entry->hostname[h]);
    memset( entry, 0, sizeof(*entry));
}

STATIC void
add_hostname( cache_entry *entry, const char *hostname)
{
    int h;
    for ( h= 0; h < 5; h++)  {
	if ( entry->hostname[h] == NULL)
	    break;
	if ( strcmp( entry->hostname[h], hostname) == 0)
	    return;
    }
    if ( h < 5)
	entry->hostname[h]= strdup( hostname);
}

/*
main(int argc, char *argv[])
{
    hcache_open( argv[1], 0);
    hcache_write(NULL);

    hcache_invalidate();
printf( "invalidate\n");
    hcache_write(NULL);
    hcache_validate();
printf( "validate\n");
    hcache_write( "/tmp/qhcache.out");
}

*/
