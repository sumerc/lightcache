
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

test:
	rm -f test_slab
	rm -f test_util
	$(CC) $(CCOPT) $(TEST_FILES) test_slab.c slab.c -o test_slab $(TEST_FLAGS) && ./test_slab
	$(CC) $(CCOPT) $(TEST_FILES) test_util.c util.c -o test_util $(TEST_FLAGS) && ./test_util
	
clean:
	rm -f $(PRGNAME)
	rm -f gmon.out
	$(CC) $(CCOPT) $(DEBUG) $(FILES) -o $(PRGNAME)

install: all
	$(INSTALL) $(PRGNAME) $(INSTALL_BIN)


	
