#!/bin/sh -x
set -e

aclocal
autoconf
autoheader
automake -a --foreign
