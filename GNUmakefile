

ifneq ($(OS),Windows_NT)
  OS := $(shell uname -s)
endif

TARGET = qstat
SRC = qstat config hcache template


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
  CFLAGS = -g
  OBJ = $(SRC:%=%.o)
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
	rm -f $(TARGET) $(OBJ)
endif

ifeq ($(OS),Windows_NT)
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OPT) $(LDFLAGS) $(OBJ) /Fe$(TARGET) $(LIBS)
else
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET) $(LIBS)
endif

.PHONY: all

qstat.o: qstat.h config.h
config.o: config.h qstat.h
template.o: qstat.h
hcache.o: qstat.h

qstat.obj: qstat.h config.h
config.obj: config.h qstat.h
template.obj: qstat.h
hcache.obj: qstat.h
