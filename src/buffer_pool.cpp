#include "buffer_pool.h"

namespace maxcso {

struct Guard {
	Guard(uv_mutex_t &mutex) : mutex_(mutex) {
		uv_mutex_lock(&mutex_);
	}
	~Guard() {
		uv_mutex_unlock(&mutex_);
	}

	uv_mutex_t &mutex_;
};

BufferPool::BufferPool() {
	uv_mutex_init(&mutex_);
}

BufferPool::~BufferPool() {
	for (uint8_t *p : free_) {
		free(p);
	}
	free_.clear();
	uv_mutex_destroy(&mutex_);
}

uint8_t *BufferPool::Alloc() {
	Guard g(mutex_);
	if (free_.empty()) {
		return static_cast<uint8_t *>(malloc(BUFFER_SIZE));
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
