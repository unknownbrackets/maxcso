#include "sector.h"
#include "cso.h"
#include "buffer_pool.h"

namespace maxcso {

Sector::Sector() : busy_(false), buffer_(nullptr), best_(nullptr) {
}

Sector::~Sector() {
	// Maybe should throw an error if it wasn't released?
	Release();
}

void Sector::Process(uv_loop_t *loop, int64_t pos, uint8_t *buffer, SectorCallback ready) {
	if (busy_) {
		ready(false, "Already busy");
		return;
	}
	busy_ = true;

	loop_ = loop;
	pos_ = pos;
	buffer_ = buffer;
	best_ = nullptr;
	ready_ = ready;
	bestSize_ = SECTOR_SIZE;

	uv_.queue_work(loop_, &work_, [this](uv_work_t *req) {
		Compress();
	}, [this](uv_work_t *req, int status) {
		if (status < 0) {
			ready_(false, "Failed to compress sector");
		} else {
			ready_(true, nullptr);
		}
	});
}

void Sector::Compress() {
	// TODO
}

void Sector::Reserve(int64_t pos, uint8_t *buffer) {
	busy_ = true;
	pos_ = pos;
	buffer_ = buffer;
}

void Sector::Release() {
	if (best_ != nullptr) {
		pool.Release(best_);
		best_ = nullptr;
	}
	if (buffer_ != nullptr) {
		pool.Release(buffer_);
	}
	buffer_ = nullptr;

	busy_ = false;
}

};
