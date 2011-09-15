
OPTIMIZATION?=-O3
DEBUG?= -pg -g -rdynamic -ggdb -D DEBUG
CFLAGS?= -std=c99 -pedantic -Wall -W -lm
TEST_FLAGS= -D LC_TEST

INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL= cp -p

FILES = lightcache.c event.c socket.c hashtab.c mem.c util.c slab.c

PRGNAME = lightcache

all: debug

debug :
	$(CC) $(CFLAGS) $(DEBUG) $(FILES) -o $(PRGNAME)

release:
	$(CC) $(CFLAGS) $(OPTIMIZATION) $(FILES) -o $(PRGNAME)
	
clean:
	rm -f $(PRGNAME)
	rm -f gmon.out
	$(CC) $(CFLAGS) $(DEBUG) $(FILES) -o $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)


	
