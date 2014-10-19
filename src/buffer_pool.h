#pragma once

#include <cstdint>
#include <vector>
#include "uv.h"

namespace maxcso {

// All buffers are 4 KB.  Might experiment with different later.
// We compress 2 KB sectors, so this is really too much.  zlib has a 5 byte overhead.
class BufferPool {
public:
	static const int BUFFER_SIZE = 4096;
	
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
