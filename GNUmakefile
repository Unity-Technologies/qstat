#CPPFLAGS=-DENABLE_DUMP -DDEBUG

-include Make.config

ifneq ($(OS),Windows_NT)
  OS := $(shell uname -s)
endif

TARGET = qstat
SRC = qstat config hcache template qserver debug md5 ut2004


ifeq ($(OS),Windows_NT)
  TARGET = qstat.exe
  OBJ = $(SRC:%=%.obj)
  LIBS = wsock32.lib
  CC = cl
  CFLAGS = /nologo /Zi
  LDFLAGS = /ML

%.obj: %.c
	$(CC) $(CFLAGS) -Fo$@ -c $*.c

else
  SYSCONFDIR = /etc
  CFLAGS ?= -O2 -g
  OBJ = $(SRC:%=%.o)
endif

ifeq ($(CC),gcc)
  CFLAGS += -MD
endif

ifeq ($(OS),Linux)
  CFLAGS += -Wall
endif

ifeq ($(OS),SunOS)
  LIBS = -lsocket -lnsl
endif

ifeq ($(OS),HP-UX)
ifneq ($(CC),gcc)
  CFLAGS += -Ae
endif
endif

ifdef SYSCONFDIR
  CFLAGS += -Dsysconfdir=\"$(SYSCONFDIR)\"
endif

all: $(TARGET)

clean:
ifeq ($(OS),Windows_NT)
	del $(TARGET) $(OBJ) *.pdb *.ilk
else
	rm -f $(TARGET) $(OBJ) *.d
endif

ifeq ($(OS),Windows_NT)
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OPT) $(LDFLAGS) $(OBJ) /Fe$(TARGET) $(LIBS)
else
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET) $(LIBS)
endif

.PHONY: all

ifneq ($(CC),gcc)
qstat.o: qstat.h config.h qserver.h debug.h ut2004.h
config.o: config.h qstat.h
template.o: qstat.h
hcache.o: qstat.h
ut2004.o: qstat.h qserver.h debug.h md5.h
debug.o: debug.h
md5.o: md5.h

qstat.obj: qstat.h config.h
config.obj: config.h qstat.h
template.obj: qstat.h
hcache.obj: qstat.h

else
-include *.d
endif

cl:
	cvs2cl.pl --utc --no-wrap --separate-header --no-times -f ChangeLog.cvs
	rm -f ChangeLog.cvs.bak

