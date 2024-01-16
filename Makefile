SRCDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CC ?= gcc
CXX ?= g++

CFLAGS ?= -O2
CXXFLAGS ?= $(CFLAGS)

PKG_CONFIG ?= pkg-config

CFLAGS_UV := $(shell $(PKG_CONFIG) --cflags libuv)
LIBS_UV := $(shell $(PKG_CONFIG) --libs libuv)

CFLAGS_LZ4 := $(shell $(PKG_CONFIG) --cflags liblz4)
LIBS_LZ4 := $(shell $(PKG_CONFIG) --libs liblz4)

CFLAGS_ZLIB := $(shell $(PKG_CONFIG) --cflags zlib)
LIBS_ZLIB := $(shell $(PKG_CONFIG) --libs zlib)

DEP_FLAGS := $(CFLAGS_UV) $(CFLAGS_LZ4) $(CFLAGS_ZLIB)
LIBS := $(LIBS_UV) $(LIBS_LZ4) $(LIBS_ZLIB)

OBJDIR := obj
MKDIRS := $(OBJDIR)/src $(OBJDIR)/cli $(OBJDIR)/zopfli/src/zopfli

SRC_CFLAGS += -W -Wall -Wextra -Wno-implicit-function-declaration -DNDEBUG=1
SRC_CXXFLAGS += -W -Wall -Wextra -std=c++11 -I$(SRCDIR)/zopfli/src -I$(SRCDIR)/7zip \
	-DNDEBUG=1 -I$(SRCDIR)/libdeflate -Wno-unused-parameter -Wno-unused-variable \
	-pthread $(DEP_FLAGS)

SRC_CXX_SRC := $(wildcard $(SRCDIR)/src/*.cpp)
SRC_CXX_TMP := $(SRC_CXX_SRC:.cpp=.o)
SRC_CXX_OBJ := $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(SRC_CXX_TMP))

CLI_CXX_SRC := $(wildcard $(SRCDIR)/cli/*.cpp)
CLI_CXX_TMP := $(CLI_CXX_SRC:.cpp=.o)
CLI_CXX_OBJ := $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(CLI_CXX_TMP))

ZOPFLI_C_DIR := $(SRCDIR)/zopfli/src/zopfli
ZOPFLI_C_SRC := $(ZOPFLI_C_DIR)/blocksplitter.c $(ZOPFLI_C_DIR)/cache.c \
               $(ZOPFLI_C_DIR)/deflate.c $(ZOPFLI_C_DIR)/gzip_container.c \
               $(ZOPFLI_C_DIR)/hash.c $(ZOPFLI_C_DIR)/katajainen.c \
               $(ZOPFLI_C_DIR)/lz77.c $(ZOPFLI_C_DIR)/squeeze.c \
               $(ZOPFLI_C_DIR)/tree.c $(ZOPFLI_C_DIR)/util.c \
               $(ZOPFLI_C_DIR)/zlib_container.c $(ZOPFLI_C_DIR)/zopfli_lib.c
ZOPFLI_C_TMP := $(ZOPFLI_C_SRC:.c=.o)
ZOPFLI_C_OBJ := $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(ZOPFLI_C_TMP))

EXTRA_LIBS =
ifeq ($(OS),Windows_NT)
	LIBDEFLATE=libdeflatestatic.lib
	EXTRA_LIBS += -luuid
else
	LIBDEFLATE=libdeflate.a
endif

SRC_7ZIP = $(OBJDIR)/7zip/7zip.a
SRC_LIBDEFLATE = $(SRCDIR)/libdeflate/$(LIBDEFLATE)

.PHONY: all clean install uninstall

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/.done
	$(CXX) -c $(SRC_CXXFLAGS) $(CXXFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)/.done
	$(CC) -c $(SRC_CFLAGS) $(CFLAGS) -o $@ $<

# TODO: Perhaps detect and use system libdeflate if available.
maxcso: $(SRC_CXX_OBJ) $(CLI_CXX_OBJ) $(ZOPFLI_C_OBJ) $(SRC_7ZIP) $(SRC_LIBDEFLATE)
	$(CXX) $(LDFLAGS) -o $@ $(SRC_CXXFLAGS) $(CXXFLAGS) $^ $(LIBS) $(EXTRA_LIBS)

$(SRC_7ZIP):
	$(MAKE) -f $(SRCDIR)/7zip/Makefile 7zip.a

$(SRC_LIBDEFLATE):
	$(MAKE) -C $(SRCDIR)/libdeflate $(LIBDEFLATE)

$(OBJDIR)/.done:
	@mkdir -p $(MKDIRS)
	@touch $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp maxcso $(DESTDIR)$(BINDIR)
	cp $(SRCDIR)/maxcso.1 $(DESTDIR)$(MANDIR)/man1
	chmod 0755 $(DESTDIR)$(BINDIR)/maxcso
	chmod 0644 $(DESTDIR)$(MANDIR)/man1/maxcso.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/maxcso
	rm -f $(DESTDIR)$(MANDIR)/man1/maxcso.1

clean:
	rm -rf -- $(OBJDIR)
	rm -f maxcso
	$(MAKE) -C $(SRCDIR)/libdeflate clean

all: maxcso
