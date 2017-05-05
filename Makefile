CC = gcc
CXX = g++

CFLAGS = -W -Wall -Wextra -O2 -Wno-implicit-function-declaration
CXXFLAGS = -W -Wall -Wextra -std=c++11 -O2 -Izopfli/src -I7zip \
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
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

maxcso: $(SRC_CXX_OBJ) $(CLI_CXX_OBJ) $(ZOPFLI_C_OBJ) 7zip/7zip.a
	$(CXX) -o $@ $(CXXFLAGS) $^ -luv -llz4 -lz

7zip/7zip.a:
	$(MAKE) -C 7zip 7zip.a

clean:
	rm -f $(SRC_CXX_OBJ) $(CLI_CXX_OBJ) $(ZOPFLI_C_OBJ) maxcso
	$(MAKE) -C 7zip clean

all: maxcso

.PHONY: clean all
