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

dist_configfiles_DATA = qstat.cfg
configfilesdir = $(sysconfdir)

EXTRA_DIST = CHANGES.txt COMPILE.txt LICENSE.txt \
	Makefile.w32 \
	ChangeLog \
	qstatdoc.html \
	contrib.cfg

cl:
	cvs2cl.pl --utc --no-wrap --separate-header --no-times -f ChangeLog.cvs
	rm -f ChangeLog.cvs.bak
