SUBDIRS = template info

CFLAGS = -Dsysconfdir=\"$(sysconfdir)\" @CFLAGS@

.PHONY: $(builddir)/.compiler_flags
$(builddir)/.compiler_flags:
	echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@

.PHONY: $(builddir)/.version
$(builddir)/.version:
	ver=$$($(srcdir)/scripts/version.sh); \
	echo $$ver | cmp -s - $@ || echo $$ver > $@

$(builddir)/version.h: $(builddir)/.version $(srcdir)/version.h.tmpl
	ver=$$(cat $<); \
	file=$$(sed "s/CHANGEME/$$ver/g" $(srcdir)/version.h.tmpl); \
	echo "$$file" | cmp -s - $@ || echo "$$file" > $@

clean-local:
	-rm -f $(builddir)/.compiler_flags $(builddir)/.version $(builddir)/version.h

distclean-local: clean-local

BUILT_SOURCES = \
	.compiler_flags \
	.version \
	version.h

bin_PROGRAMS = qstat

qstat_DEPENDENCIES = \
	$(builddir)/.compiler_flags

qstat_SOURCES = \
	version.h.tmpl \
	version.h \
	xform.c xform.h \
	config.c config.h \
	debug.c debug.h \
	utils.c utils.h \
	hcache.c \
	md5.c md5.h \
	qserver.c qserver.h \
	qstat.c qstat.h \
	template.c \
	display_json.c display_json.h \
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
	crysis.c crysis.h \
	dirtybomb.c dirtybomb.h \
	starmade.c starmade.h \
	farmsim.c farmsim.h \
	ksp.c ksp.h \
	tf.c tf.h \
	armyops.c armyops.h \
	skulltag_huffman.c skulltag_huffman.h \
	zandronum.c zandronum.h

dist_configfiles_DATA = qstat.cfg
configfilesdir = $(sysconfdir)

EXTRA_DIST = CHANGES.txt COMPILE.md LICENSE.txt \
	Makefile.noauto \
	ChangeLog \
	qstatdoc.html \
	contrib.cfg \
	scripts/version.sh
