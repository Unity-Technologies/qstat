#!/bin/sh -x

if which autoreconf >/dev/null 2>&1; then
	autoreconf -i
else
	aclocal
	autoconf
	autoheader
	automake
fi
