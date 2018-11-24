CC ?= gcc
CXX ?= g++
AR ?= ar

SRC_CFLAGS = -W -Wall -Wextra
SRC_CXXFLAGS = -W -Wall -Wextra -std=c++11 -ICPP

CFLAGS ?= -O2
CXXFLAGS ?= $(CFLAGS)

7ZIP_CXX_SRC = CPP/7zip/Archive/Common/ParseProperties.cpp \
               CPP/7zip/Archive/DeflateProps.cpp \
               CPP/7zip/Common/CWrappers.cpp \
               CPP/7zip/Common/FileStreams.cpp \
               CPP/7zip/Common/InBuffer.cpp \
               CPP/7zip/Common/OutBuffer.cpp \
               CPP/7zip/Common/StreamUtils.cpp \
               CPP/7zip/Compress/BitlDecoder.cpp \
               CPP/7zip/Compress/DeflateDecoder.cpp \
               CPP/7zip/Compress/DeflateEncoder.cpp \
               CPP/7zip/Compress/LzOutWindow.cpp \
               CPP/7zip/Compress/ZlibDecoder.cpp \
               CPP/7zip/Compress/ZlibEncoder.cpp \
               CPP/Common/MyString.cpp \
               CPP/Common/StringConvert.cpp \
               CPP/Common/StringToInt.cpp \
               CPP/NotWindows/FileIO.cpp \
               CPP/NotWindows/MyWindows.cpp \
               CPP/Windows/PropVariant.cpp \
               deflate7z.cpp
7ZIP_CXX_OBJ = $(7ZIP_CXX_SRC:.cpp=.o)
7ZIP_C_SRC =   C/Alloc.c \
               C/HuffEnc.c \
               C/LzFind.c \
               C/Sort.c
7ZIP_C_OBJ =   $(7ZIP_C_SRC:.c=.o)

%.o: %.cpp
	$(CXX) -c $(SRC_CXXFLAGS) $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(SRC_CFLAGS) $(CFLAGS) -o $@ $<

7zip.a: $(7ZIP_CXX_OBJ) $(7ZIP_C_OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(7ZIP_CXX_OBJ) $(7ZIP_C_OBJ) 7zip.a

all: 7zip.a

.PHONY: clean all
