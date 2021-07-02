#!/bin/sh

if [ "$QSTAT_VERSION" = "" ]; then
	QSTAT_VERSION=`git describe --tags --always`
fi

# Detect if our git repo is in a dirty state (uncommitted changes)
GIT_DIRTY_FILE=`git status --porcelain 2>/dev/null| grep -Fv 'gnuconfig.h.in~' | egrep "^(M| M|\?\?)" | wc -l | sed -e 's/^[[:space:]]*//'`
if [ ${GIT_DIRTY_FILE} -ne 0 ]; then
	QSTAT_VERSION="$QSTAT_VERSION-modified"
fi

echo -n $QSTAT_VERSION
