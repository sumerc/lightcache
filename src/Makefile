
OPTIMIZATION?=-O3
CFLAGS?=  -std=c99 -pedantic $(OPTIMIZATION) -Wall -W 

CCOPT= $(CFLAGS)
DEBUG?= -g -rdynamic -ggdb -D LITTLE_ENDIAN -D DEBUG

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL= cp -p

FILES = lightcache.c events/event.c socket.c hashtab.c mem.c freelist.c util.c

PRGNAME = lightcache

all: lightcache_server

lightcache_server :
	$(CC) -o $(PRGNAME) $(CCOPT) $(DEBUG) $(FILES)
	
clean:
	rm $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)
	
