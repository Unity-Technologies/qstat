SUBDIRS = template info

CPPFLAGS = -Dsysconfdir=\"$(sysconfdir)\" @CPPFLAGS@

bin_PROGRAMS = qstat

qstat_SOURCES = \
	config.c config.h \
	debug.c debug.h \
	hcache.c \
	md5.c md5.h \
	qserver.c qserver.h \
	qstat.c qstat.h \
	template.c \
	ut2004.c ut2004.h

EXTRA_DIST = CHANGES.txt COMPILE.txt COPYING ChangeLog INSTALL LICENSE.txt

cl:
	cvs2cl.pl --utc --no-wrap --separate-header --no-times -f ChangeLog.cvs
	rm -f ChangeLog.cvs.bak
