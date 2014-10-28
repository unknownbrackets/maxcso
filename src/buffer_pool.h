#pragma once

#include <cstdint>
#include <vector>
#include "uv.h"

namespace maxcso {

// All buffers are the same size.  This just makes things simpler.
// zlib has a 5 byte overhead, so we really only need them to be double the size of
// our block size.  We also decompress DAX 8KB buffers, so we need at least that much.
class BufferPool {
public:
	uint32_t bufferSize;
	
	BufferPool();
	~BufferPool();

	bool SetBufferSize(uint32_t newSize);
	uint8_t *Alloc();
	void Release(uint8_t *p);

private:
	void Clear();

	size_t allocations_;
	std::vector<uint8_t *> free_;
	uv_mutex_t mutex_;
};

extern BufferPool pool;

};
