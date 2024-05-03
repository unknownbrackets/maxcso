cmake_minimum_required(VERSION 3.5)

set(ZOPFLI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zopfli/src/zopfli)
add_library(
	zopfli
	OBJECT
	${ZOPFLI_SRC_DIR}/blocksplitter.c
	${ZOPFLI_SRC_DIR}/cache.c
	${ZOPFLI_SRC_DIR}/deflate.c
	${ZOPFLI_SRC_DIR}/gzip_container.c
	${ZOPFLI_SRC_DIR}/hash.c
	${ZOPFLI_SRC_DIR}/katajainen.c
	${ZOPFLI_SRC_DIR}/lz77.c
	${ZOPFLI_SRC_DIR}/squeeze.c
	${ZOPFLI_SRC_DIR}/tree.c
	${ZOPFLI_SRC_DIR}/util.c
	${ZOPFLI_SRC_DIR}/zlib_container.c
	${ZOPFLI_SRC_DIR}/zopfli_lib.c
)
target_include_directories(zopfli PUBLIC ${ZOPFLI_SRC_DIR}/..)
