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

enum SectorFormat {
	SECTOR_FMT_ORIG,
	SECTOR_FMT_DEFLATE,
	SECTOR_FMT_LZ4,
};

// Actually block.
class Sector {
public:
	Sector(uint32_t flags);
	~Sector();

	void Setup(uv_loop_t *loop, uint32_t blockSize, uint32_t align, uint32_t origMaxCost, uint32_t lz4MaxCost) {
		loop_ = loop;
		blockSize_ = blockSize;
		align_ = align;
		origMaxCost_ = origMaxCost;
		lz4MaxCost_ = lz4MaxCost;
	}

	void Process(int64_t pos, uint8_t *buffer, SectorCallback ready);
	// Call after Process() or Release().
	void Release();
	void DisableCompress() {
		compress_ = false;
	}

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
	SectorFormat Format() {
		return bestFmt_;
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
	void LZ4HCTrial(bool allowBrute);
	void LZ4Trial();
	bool SubmitTrial(uint8_t *result, uint32_t size, SectorFormat fmt);

	UVHelper uv_;
	uv_loop_t *loop_;
	uint32_t flags_;
	uint32_t align_;
	uint32_t origMaxCost_;
	uint32_t lz4MaxCost_;
	bool busy_;
	bool enqueued_;
	bool compress_;

	uint32_t blockSize_;
	uint32_t readySize_;

	int64_t pos_;
	uint8_t *buffer_;
	uint8_t *best_;
	uint32_t bestSize_;
	SectorFormat bestFmt_;

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
