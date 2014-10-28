#include "buffer_pool.h"
#include "uv_helper.h"

namespace maxcso {

static const uint32_t MIN_SIZE = 16384;

BufferPool::BufferPool() : bufferSize(MIN_SIZE), allocations_(0) {
	uv_mutex_init(&mutex_);
}

BufferPool::~BufferPool() {
	Clear();
	uv_mutex_destroy(&mutex_);
}

void BufferPool::Clear() {
	for (uint8_t *p : free_) {
		free(p);
	}
	allocations_ -= free_.size();
	free_.clear();
}

bool BufferPool::SetBufferSize(uint32_t newSize) {
	Guard g(mutex_);
	if (newSize < MIN_SIZE) {
		newSize = MIN_SIZE;
	}
	Clear();

	// Only if there are no buffers "in the wind" can this work.
	if (allocations_ == 0) {
		bufferSize = newSize;
		return true;
	}
	return false;
}

uint8_t *BufferPool::Alloc() {
	Guard g(mutex_);
	if (free_.empty()) {
		++allocations_;
		return static_cast<uint8_t *>(malloc(bufferSize));
	}
	uint8_t *p = free_.back();
	free_.pop_back();
	return p;
}

void BufferPool::Release(uint8_t *p) {
	Guard g(mutex_);
	free_.push_back(p);
}

BufferPool pool;

};
