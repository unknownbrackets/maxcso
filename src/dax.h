#pragma once

#include <cstdint>

namespace maxcso {

static const char *DAX_MAGIC = "DAX\0";
static const uint32_t DAX_FRAME_SIZE = 0x2000;
static const uint32_t DAX_FRAME_MASK = 0x1FFF;
static const uint8_t DAX_FRAME_SHIFT = 13;

// TODO: Endian-ify?
struct DAXHeader {
	char magic[4];
	uint32_t uncompressed_size;
	uint32_t version;
	uint32_t nc_areas;
	uint32_t unused[4];
};

struct DAXNCArea {
	uint32_t start;
	uint32_t count;
};

};