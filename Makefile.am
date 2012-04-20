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
	a2s.c a2s.h \
	packet_manip.c packet_manip.h \
	ut2004.c ut2004.h \
	doom3.c doom3.h \
	gps.c gps.h \
	gs2.c gs2.h \
	gs3.c gs3.h \
	ts2.c ts2.h \
	tm.c tm.h \
	haze.c haze.h \
	ottd.c ottd.h \
	wic.c wic.h \
	fl.c fl.h \
	tee.c tee.h \
	cube2.c cube2.h \
	ts3.c ts3.h \
	bfbc2.c bfbc2.h \
	ventrilo.c ventrilo.h \
	mumble.c mumble.h \
	terraria.c terraria.h \
	crysis.c crysis.h

dist_configfiles_DATA = qstat.cfg
configfilesdir = $(sysconfdir)

EXTRA_DIST = CHANGES.txt COMPILE.txt LICENSE.txt \
	Makefile.noauto \
	ChangeLog \
	qstatdoc.html \
	contrib.cfg

cl:
	cvs2cl.pl --utc --no-wrap --separate-header --no-times -f ChangeLog.cvs
	rm -f ChangeLog.cvs.bak qstat.core
