
OPTIMIZATION?=-O3
DEBUG?= -pg -g -rdynamic -ggdb -D LITTLE_ENDIAN -D DEBUG
CFLAGS?= -std=c99 -pedantic -Wall -W

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL= cp -p

FILES = lightcache.c event.c socket.c hashtab.c mem.c freelist.c util.c

PRGNAME = lightcache

all: debug

debug :
	$(CC) $(CFLAGS) $(DEBUG) $(FILES) -o $(PRGNAME)

release:
	$(CC) $(CFLAGS) $(OPTIMIZATION)  $(FILES) -o $(PRGNAME)
	
clean:
	rm $(PRGNAME)
	rm gmon.out
	$(CC) $(CFLAGS) $(DEBUG) $(FILES) -o $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)
	
