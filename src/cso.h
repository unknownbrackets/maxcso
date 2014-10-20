#pragma once

#include <cstdint>

namespace maxcso {

static const char *CSO_MAGIC = "CISO";
static const uint32_t CSO_INDEX_UNCOMPRESSED = 0x80000000;

static const uint32_t SECTOR_SIZE = 0x800;
static const uint32_t SECTOR_MASK = 0x7FF;
static const uint8_t SECTOR_SHIFT = 11;

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__((__packed__))
#endif

// TODO: Endian-ify?
struct CSOHeader {
	char magic[4];
	uint32_t header_size;
	uint64_t uncompressed_size;
	uint32_t sector_size;
	uint8_t version;
	uint8_t index_shift;
	uint8_t unused[2];
} PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef PACKED

};
