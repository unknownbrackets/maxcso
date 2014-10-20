#pragma once

#include <cstdint>
#include <functional>
#include "uv_helper.h"

namespace maxcso {

typedef std::function<void (bool status, const char *reason)> SectorCallback;

class Sector {
public:
	Sector();
	~Sector();

	void Process(uv_loop_t *loop, int64_t pos, uint8_t *buffer, SectorCallback ready);
	// For when compression is not desired, e.g. for fast sectors.
	// This still marks as busy so WriteReq() can be used.
	void Reserve(int64_t pos, uint8_t *buffer);
	// Call after Process() or Release().
	void Release();

	uint8_t *BestBuffer() {
		return best_ == nullptr ? buffer_ : best_;
	}
	uint32_t BestSize() {
		return bestSize_;
	}
	bool Compressed() {
		return best_ != nullptr;
	}

	// Just so it has some place to live.
	// Otherwise, Output needs to handle a list of these.
	uv_fs_t *WriteReq() {
		return &write_;
	}

private:
	void Compress();

	UVHelper uv_;
	uv_loop_t *loop_;
	bool busy_;

	int64_t pos_;
	uint8_t *buffer_;
	uint8_t *best_;
	uint32_t bestSize_;

	uv_work_t work_;
	uv_fs_t write_;

	SectorCallback ready_;
};

};
