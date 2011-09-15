
OPTIMIZATION?=-O3
CFLAGS?=  -std=c99 -pedantic $(OPTIMIZATION) -Wall -W -lm

CCOPT= $(CFLAGS)
DEBUG?= -pg -g -rdynamic -ggdb -D DEBUG
TEST_FILES= test_base.c
TEST_FLAGS= -D LC_TEST

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL= cp -p

FILES = lightcache.c event.c socket.c hashtab.c mem.c util.c slab.c

PRGNAME = lightcache

all: lightcache_server

lightcache_server :
	$(CC) $(CCOPT) $(DEBUG) $(FILES) -o $(PRGNAME)

clean:
	rm -f $(PRGNAME)
	rm -f gmon.out
	$(CC) $(CCOPT) $(DEBUG) $(FILES) -o $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)


	
