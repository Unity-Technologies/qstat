/*
 * config.h
 * by Steve Jankowski
 * steve@qstat.org
 * http://www.qstat.org
 *
 * Copyright 1996,1997,1998,1999 by Steve Jankowski
 */


#include "qstat.h"

int qsc_load_default_config_files();
int qsc_load_config_file( char *filename);
server_type ** qsc_get_config_server_types( int *n_config_types);


