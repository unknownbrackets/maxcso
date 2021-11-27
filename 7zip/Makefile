SRCDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

CC ?= gcc
CXX ?= g++
AR ?= ar

SRC_CFLAGS = -W -Wall -Wextra
SRC_CXXFLAGS = -W -Wall -Wextra -std=c++11 -I$(SRCDIR)/CPP

CFLAGS ?= -O2
CXXFLAGS ?= $(CFLAGS)

OBJDIR := obj
BLDDIR := $(OBJDIR)/7zip
MKDIRS := \
	$(BLDDIR)/CPP/7zip/Archive/Common \
	$(BLDDIR)/CPP/7zip/Common \
	$(BLDDIR)/CPP/7zip/Compress \
	$(BLDDIR)/CPP/Common \
	$(BLDDIR)/CPP/NotWindows \
	$(BLDDIR)/CPP/Windows \
	$(BLDDIR)/C

7ZIP_ARCHIVE_DIR := $(SRCDIR)/CPP/7zip/Archive
7ZIP_COMMON_DIR := $(SRCDIR)/CPP/7zip/Common
7ZIP_COMPRESS_DIR := $(SRCDIR)/CPP/7zip/Compress

7ZIP_CXX_SRC := $(7ZIP_ARCHIVE_DIR)/Common/ParseProperties.cpp \
                $(7ZIP_ARCHIVE_DIR)/DeflateProps.cpp \
                $(7ZIP_COMMON_DIR)/CWrappers.cpp \
                $(7ZIP_COMMON_DIR)/FileStreams.cpp \
                $(7ZIP_COMMON_DIR)/InBuffer.cpp \
                $(7ZIP_COMMON_DIR)/OutBuffer.cpp \
                $(7ZIP_COMMON_DIR)/StreamUtils.cpp \
                $(7ZIP_COMPRESS_DIR)/BitlDecoder.cpp \
                $(7ZIP_COMPRESS_DIR)/DeflateDecoder.cpp \
                $(7ZIP_COMPRESS_DIR)/DeflateEncoder.cpp \
                $(7ZIP_COMPRESS_DIR)/LzOutWindow.cpp \
                $(7ZIP_COMPRESS_DIR)/ZlibDecoder.cpp \
                $(7ZIP_COMPRESS_DIR)/ZlibEncoder.cpp \
                $(SRCDIR)/CPP/Common/MyString.cpp \
                $(SRCDIR)/CPP/Common/StringConvert.cpp \
                $(SRCDIR)/CPP/Common/StringToInt.cpp \
                $(SRCDIR)/CPP/NotWindows/FileIO.cpp \
                $(SRCDIR)/CPP/NotWindows/MyWindows.cpp \
                $(SRCDIR)/CPP/Windows/PropVariant.cpp \
                $(SRCDIR)/deflate7z.cpp
7ZIP_CXX_TMP := $(7ZIP_CXX_SRC:.cpp=.o)
7ZIP_CXX_OBJ := $(patsubst $(SRCDIR)/%,$(BLDDIR)/%,$(7ZIP_CXX_TMP))


7ZIP_C_SRC :=  $(SRCDIR)/C/Alloc.c \
               $(SRCDIR)/C/HuffEnc.c \
               $(SRCDIR)/C/LzFind.c \
               $(SRCDIR)/C/Sort.c
7ZIP_C_TMP :=  $(7ZIP_C_SRC:.c=.o)
7ZIP_C_OBJ := $(patsubst $(SRCDIR)/%,$(BLDDIR)/%,$(7ZIP_C_TMP))

$(BLDDIR)/%.o: $(SRCDIR)/%.cpp $(BLDDIR)/.done
	$(CXX) -c $(SRC_CXXFLAGS) $(CXXFLAGS) -o $@ $<

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(BLDDIR)/.done
	$(CC) -c $(SRC_CFLAGS) $(CFLAGS) -o $@ $<

7zip.a: $(7ZIP_CXX_OBJ) $(7ZIP_C_OBJ)
	$(AR) rcs $(BLDDIR)/$@ $^

$(BLDDIR)/.done:
	@mkdir -p $(MKDIRS)
	@touch $@

clean:
	rm -rf -- $(OBJDIR)

all: 7zip.a

.PHONY: clean all
