#pragma once

#include <cstdint>
#include <functional>
#include "uv_helper.h"

typedef struct z_stream_s z_stream;

namespace Deflate7z {
	struct Context;
};

namespace maxcso {

typedef std::function<void (bool status, const char *reason)> SectorCallback;

class Sector {
public:
	Sector(uint32_t flags);
	~Sector();

	void Process(uv_loop_t *loop, int64_t pos, uint8_t *buffer, uint32_t align, SectorCallback ready);
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
	int64_t Pos() {
		return pos_;
	}

	// Just so it has some place to live.
	// Otherwise, Output needs to handle a list of these.
	uv_fs_t *WriteReq() {
		return &write_;
	}

private:
	uint32_t AlignedBestSize(uint32_t align) {
		const uint32_t off = bestSize_ % align;
		if (off != 0) {
			return bestSize_ + align - off;
		}
		return bestSize_;
	}

	void Compress();
	void FinalizeBest(uint32_t align);
	void ZlibTrial(z_stream *z);
	void ZopfliTrial();
	void SevenZipTrial();
	bool SubmitTrial(uint8_t *result, uint32_t size);

	UVHelper uv_;
	uv_loop_t *loop_;
	uint32_t flags_;
	bool busy_;

	int64_t pos_;
	uint8_t *buffer_;
	uint8_t *best_;
	uint32_t bestSize_;

	uv_work_t work_;
	uv_fs_t write_;

	SectorCallback ready_;

	z_stream *zDefault_;
	z_stream *zFiltered_;
	z_stream *zHuffman_;
	z_stream *zRLE_;
	Deflate7z::Context *deflate7z_;
};

};
