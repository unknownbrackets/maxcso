cmake_minimum_required(VERSION 3.8)
project(
	maxcso
	VERSION 1.13.0
	DESCRIPTION
		"A fast ISO to CSO compression program for use with PSP and PS2 emulators"
	HOMEPAGE_URL https://github.com/unknownbrackets/maxcso
	LANGUAGES C CXX
)


# Prevent cmake from running in the root directory to avoid replacing the original Makefile
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Generating CMake build files in the project root directory is not allowed. Please create a new directory and run cmake from there, or use the command 'cmake -B /path/to/build_directory' to specify a build directory.")
endif()


set(CMAKE_CXX_STANDARD 11)
add_compile_options(
	-W
	-Wall
	-Wextra
	"$<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>"
	"$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter;-Wno-unused-variable>"
)


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBDEFLATE REQUIRED libdeflate)
pkg_check_modules(LIBUV REQUIRED libuv)
pkg_check_modules(LZ4 REQUIRED liblz4)
pkg_check_modules(ZLIB REQUIRED zlib)

if(WIN32)
	pkg_check_modules(UUID uuid REQUIRED)
endif()

# Add 7zip and zopfli libraries
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(7zip)
include(Zopfli)


add_executable(
	maxcso
	cli/cli.cpp
	cli/winargs.cpp
	cli/winglob.cpp
	src/buffer_pool.cpp
	src/checksum.cpp
	src/compress.cpp
	src/input.cpp
	src/output.cpp
	src/sector.cpp
	src/uv_helper.cpp
)
target_include_directories(
	maxcso
	PRIVATE
		${LIBDEFLATE_INCLUDE_DIRS}
		${LIBUV_INCLUDE_DIRS}
		${LZ4_INCLUDE_DIRS}
		${UUID_INCLUDE_DIRS}
		${ZLIB_INCLUDE_DIRS}
)
target_link_libraries(
	maxcso
	PRIVATE
		${LIBDEFLATE_LIBRARIES}
		${LIBUV_LIBRARIES}
		${LZ4_LIBRARIES}
		${UUID_LIBRARIES}
		${ZLIB_LIBRARIES}
		7zip
		zopfli
		Threads::Threads
)


include(GNUInstallDirs)
install(TARGETS maxcso DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
	FILES ${CMAKE_SOURCE_DIR}/maxcso.1
	DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)
