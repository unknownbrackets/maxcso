#pragma once

#include <cstdint>
#include <vector>
#include "uv.h"

namespace maxcso {

// All buffers are 16 KB.  Might experiment with different later.
// We compress 2 KB sectors, so this is really too much.  zlib has a 5 byte overhead.
// However, we also decompress DAX 8KB buffers.
class BufferPool {
public:
	static const int BUFFER_SIZE = 16384;
	
	BufferPool();
	~BufferPool();

	uint8_t *Alloc();
	void Release(uint8_t *p);

private:
	std::vector<uint8_t *> free_;
	uv_mutex_t mutex_;
};

extern BufferPool pool;

};
