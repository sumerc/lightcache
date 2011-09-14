
OPTIMIZATION?=-O3
CFLAGS?=  -std=c99 -pedantic $(OPTIMIZATION) -Wall -W -lm

CCOPT= $(CFLAGS)
DEBUG?= -pg -g -rdynamic -ggdb -D LITTLE_ENDIAN -D DEBUG

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL= cp -p

FILES = lightcache.c event.c socket.c hashtab.c mem.c util.c slab.c

PRGNAME = lightcache

all: lightcache_server

lightcache_server :
	$(CC) $(CCOPT) $(DEBUG) $(FILES) -o $(PRGNAME)

test:
	rm -f test_slab
	$(CC) $(CCOPT) test_slab.c slab.c -o test_slab -DTEST -lm && ./test_slab
	../etc/autotest.sh
	
clean:
	rm -f $(PRGNAME)
	rm -f gmon.out
	$(CC) $(CCOPT) $(DEBUG) $(FILES) -o $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)


	
