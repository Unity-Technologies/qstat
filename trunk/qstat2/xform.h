/*
 * xform Functions
 * Copyright 2013 Steven Hartland
 *
 * Licensed under the Artistic License, see LICENSE.txt for license terms
 */
#ifndef QSTAT_XFORM_H
#define QSTAT_XFORM_H

#include "qstat.h"

void xform_buf_free();
int xform_printf(FILE *, const char *, ...);
char * xform_name(char *, struct qserver *);

#endif
