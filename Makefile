PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CC ?= gcc
CXX ?= g++

CFLAGS ?= -O2
CXXFLAGS ?= $(CFLAGS)

SRC_CFLAGS += -W -Wall -Wextra -Wno-implicit-function-declaration -DNDEBUG=1
SRC_CXXFLAGS += -W -Wall -Wextra -std=c++11 -Izopfli/src -I7zip -DNDEBUG=1 \
	-Wno-unused-parameter -pthread

SRC_CXX_SRC = $(wildcard src/*.cpp)
SRC_CXX_OBJ = $(SRC_CXX_SRC:.cpp=.o)
CLI_CXX_SRC = $(wildcard cli/*.cpp)
CLI_CXX_OBJ = $(CLI_CXX_SRC:.cpp=.o)
ZOPFLI_C_SRC = zopfli/src/zopfli/blocksplitter.c zopfli/src/zopfli/cache.c \
               zopfli/src/zopfli/deflate.c zopfli/src/zopfli/gzip_container.c \
               zopfli/src/zopfli/hash.c zopfli/src/zopfli/katajainen.c \
               zopfli/src/zopfli/lz77.c zopfli/src/zopfli/squeeze.c \
               zopfli/src/zopfli/tree.c zopfli/src/zopfli/util.c \
               zopfli/src/zopfli/zlib_container.c zopfli/src/zopfli/zopfli_lib.c
ZOPFLI_C_OBJ = $(ZOPFLI_C_SRC:.c=.o)

%.o: %.cpp
	$(CXX) -c $(SRC_CXXFLAGS) $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(SRC_CFLAGS) $(CFLAGS) -o $@ $<

maxcso: $(SRC_CXX_OBJ) $(CLI_CXX_OBJ) $(ZOPFLI_C_OBJ) 7zip/7zip.a
	$(CXX) -o $@ $(SRC_CXXFLAGS) $(CXXFLAGS) $^ -luv -llz4 -lz

7zip/7zip.a:
	$(MAKE) -C 7zip 7zip.a

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp maxcso $(DESTDIR)$(BINDIR)
	cp maxcso.1 $(DESTDIR)$(MANDIR)/man1
	chmod 0755 $(DESTDIR)$(BINDIR)/maxcso
	chmod 0644 $(DESTDIR)$(MANDIR)/man1/maxcso.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/maxcso
	rm -f $(DESTDIR)$(MANDIR)/man1/maxcso.1

clean:
	rm -f $(SRC_CXX_OBJ) $(CLI_CXX_OBJ) $(ZOPFLI_C_OBJ) maxcso
	$(MAKE) -C 7zip clean

all: maxcso

.PHONY: clean all
