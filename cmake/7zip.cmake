cmake_minimum_required(VERSION 3.8)

set(7ZIP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/7zip)
add_library(
	7zip
	STATIC
	${7ZIP_SRC_DIR}/C/Alloc.c
	${7ZIP_SRC_DIR}/C/HuffEnc.c
	${7ZIP_SRC_DIR}/C/LzFind.c
	${7ZIP_SRC_DIR}/C/Sort.c
	${7ZIP_SRC_DIR}/CPP/7zip/Archive/Common/ParseProperties.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Archive/DeflateProps.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Common/CWrappers.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Common/FileStreams.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Common/InBuffer.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Common/OutBuffer.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Common/StreamUtils.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/BitlDecoder.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/DeflateDecoder.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/DeflateEncoder.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/LzOutWindow.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/ZlibDecoder.cpp
	${7ZIP_SRC_DIR}/CPP/7zip/Compress/ZlibEncoder.cpp
	${7ZIP_SRC_DIR}/CPP/Common/MyString.cpp
	${7ZIP_SRC_DIR}/CPP/Common/StringConvert.cpp
	${7ZIP_SRC_DIR}/CPP/Common/StringToInt.cpp
	${7ZIP_SRC_DIR}/CPP/NotWindows/FileIO.cpp
	${7ZIP_SRC_DIR}/CPP/NotWindows/MyWindows.cpp
	${7ZIP_SRC_DIR}/CPP/Windows/PropVariant.cpp
	${7ZIP_SRC_DIR}/deflate7z.cpp
)
target_include_directories(7zip PUBLIC ${7ZIP_SRC_DIR} ${7ZIP_SRC_DIR}/CPP)
