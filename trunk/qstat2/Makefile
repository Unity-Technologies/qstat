
CFLAGS = -ggdb

# Uncomment if you have gcc
CC = gcc

# NOTE: if you get errors when linking qstat (missing symbols or
# libraries), then switch the LIBS macro being used.

SRC = qstat.c config.c hcache.c template.c debug.c ut2004.c md5.c qserver.c
O = qstat.o config.o hcache.o template.o
OBJ = qstat.obj config.obj hcache.obj template.obj

NO_LIBS =
SOLARIS_LIBS = -lsocket -lnsl
WINDOWS_LIBS = wsock32.lib
OS2_LIBS = so32dll.lib tcp32dll.lib
EMX_LIBS = -lsocket

# Irix 5.3, Linux, FreeBSD, many other Unixes
LIBS = $(NO_LIBS)

# Solaris 2
# LIBS = $(SOLARIS_LIBS)

# Windows 95/NT
# LIBS = $(WINDOWS_LIBS)

# OS/2
# LIBS = $(OS2_LIBS)

# The first line is for Unix.  Switch the comment character for Windows.
# (there should be no need to compile on Windows, but in case you care ...)
all: qstat
#all: qstat.exe

qstat: $(SRC)
	$(CC) $(CFLAGS) -o qstat $(SRC) $(LIBS)

solaris: $(SRC)
	$(CC) $(CFLAGS) -o qstat $(SRC) $(SOLARIS_LIBS)

aix sgi freebsd macosx osx openbsd irix linux: $(SRC)
	$(CC) $(CFLAGS) -o qstat $(SRC) $(NO_LIBS)

hp hpux: $(SRC)
	$(CC) $(CFLAGS) -Ae -o qstat $(SRC) $(NO_LIBS)

windows: $(SRC)
	$(CC) $(CFLAGS) /ML $(SRC) /Feqstat.exe $(WINDOWS_LIBS)

windows_debug: $(SRC)
	rm -f *.pdb
	$(CC) $(CFLAGS) /Zi /ML $(SRC) /Feqstat.exe wsock32.lib /link /fixed:no /incremental:no /pdbtype:con

os2: $(SRC)
	$(CC) /Q /W0 /C+ $(SRC)
	ilink /PM:VIO $(OBJ) /out:qstat.exe $(OS2_LIBS)

os2emx: $(SRC)
	$(CC) $(CFLAGS) -o qstat.exe $(SRC) $(EMX_LIBS)

clean:
	rm -f qstat core qstat.exe $(O) $(OBJ)

VERSION = 25c
WVERSION = 25c
CP_FILES = CHANGES.txt LICENSE.txt qstatdoc.html \
	Makefile GNUmakefile qstat.cfg qstat.c qstat.h \
	config.c config.h hcache.c template.c \
	COMPILE.txt contrib.cfg info template

tar: always
	rm -rf tar qstat$(VERSION)
	rm -f qstat$(VERSION).tar qstat$(VERSION).tar.Z qstat$(VERSION).tar.gz
	mkdir qstat$(VERSION)
	-cp -rp $(CP_FILES) qstat$(VERSION)
	cd qstat$(VERSION) ; chmod u+w qstat.cfg Makefile GNUmakefile ; cd ..
	tar cvf qstat$(VERSION).tar qstat$(VERSION)
	gzip qstat$(VERSION).tar

zip: always
	rm -rf zip qsta$(WVERSION) qstat$(WVERSION)
	rm -f qstat$(WVERSION).zip qstat$(WVERSION).zip
	mkdir qstat$(WVERSION)
	-cp -rp $(CP_FILES) win32 qstat$(WVERSION)
	mv qstat$(WVERSION)/qstatdoc.html qstat$(WVERSION)/qstatdoc.htm
	mv qstat$(WVERSION)/template/brocTh.html \
		qstat$(WVERSION)/template/brocTh.htm
	mv qstat$(WVERSION)/template/brocTp.html \
		qstat$(WVERSION)/template/brocTp.htm
	mv qstat$(WVERSION)/template/brocTs.html \
		qstat$(WVERSION)/template/brocTs.htm
	mv qstat$(WVERSION)/template/brocTt.html \
		qstat$(WVERSION)/template/brocTt.htm
	mv qstat$(WVERSION)/template/ghostreconTh.html \
		qstat$(WVERSION)/template/ghostreconTh.htm
	mv qstat$(WVERSION)/template/ghostreconTp.html \
		qstat$(WVERSION)/template/ghostreconTp.htm
	mv qstat$(WVERSION)/template/ghostreconTs.html \
		qstat$(WVERSION)/template/ghostreconTs.htm
	mv qstat$(WVERSION)/template/ghostreconTt.html \
		qstat$(WVERSION)/template/ghostreconTt.htm
	mv qstat$(WVERSION)/template/unrealTh.html \
		qstat$(WVERSION)/template/unrealTh.htm
	mv qstat$(WVERSION)/template/unrealTp.html \
		qstat$(WVERSION)/template/unrealTp.htm
	mv qstat$(WVERSION)/template/unrealTs.html \
		qstat$(WVERSION)/template/unrealTs.htm
	mv qstat$(WVERSION)/template/unrealTt.html \
		qstat$(WVERSION)/template/unrealTt.htm
	mv qstat$(WVERSION)/template/tribes2th.html \
		qstat$(WVERSION)/template/tribes2th.htm
	mv qstat$(WVERSION)/template/tribes2tp.html \
		qstat$(WVERSION)/template/tribes2tp.htm
	mv qstat$(WVERSION)/template/tribes2ts.html \
		qstat$(WVERSION)/template/tribes2ts.htm
	mv qstat$(WVERSION)/template/tribes2tt.html \
		qstat$(WVERSION)/template/tribes2tt.htm
	cd qstat$(WVERSION) ; crtocrlf * ; cd ..
	cd qstat$(WVERSION)/template ; crtocrlf * ; cd ../..
	cd qstat$(WVERSION)/info ; crtocrlf * ; cd ../..
	zip -r qstat$(WVERSION).zip qstat$(WVERSION)

always:
