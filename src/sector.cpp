#include "sector.h"
#include "compress.h"
#include "cso.h"
#include "buffer_pool.h"

namespace maxcso {

Sector::Sector(uint32_t flags) : flags_(flags), busy_(false), buffer_(nullptr), best_(nullptr) {
	// Set up the zlib streams, which we will reuse each time we hit this sector.
	memset(&zDefault_, 0, sizeof(zDefault_));
	deflateInit2(&zDefault_, 9, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
	memset(&zFiltered_, 0, sizeof(zDefault_));
	deflateInit2(&zFiltered_, 9, Z_DEFLATED, -15, 9, Z_FILTERED);
	memset(&zHuffman_, 0, sizeof(zDefault_));
	deflateInit2(&zHuffman_, 9, Z_DEFLATED, -15, 9, Z_HUFFMAN_ONLY);
	memset(&zRLE_, 0, sizeof(zDefault_));
	deflateInit2(&zRLE_, 9, Z_DEFLATED, -15, 9, Z_RLE);
}

Sector::~Sector() {
	// Maybe should throw an error if it wasn't released?
	Release();

	deflateEnd(&zDefault_);
	deflateEnd(&zFiltered_);
	deflateEnd(&zHuffman_);
	deflateEnd(&zRLE_);
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
	// Each of these sometimes wins on certain blocks.
	if (!(flags_ & TASKFLAG_NO_ZLIB_DEFAULT)) {
		ZlibTrial(&zDefault_);
	}
	if (!(flags_ & TASKFLAG_NO_ZLIB_BRUTE)) {
		ZlibTrial(&zFiltered_);
		ZlibTrial(&zHuffman_);
		ZlibTrial(&zRLE_);
	}
}

// TODO: Split these out to separate files?
void Sector::ZlibTrial(z_stream *z) {
	// TODO: Validate the benefit of these with raw on msvc and gcc.
	// Try TOO_FAR?  May not matter much for 2048 bytes.
	// http://jsnell.iki.fi/blog/
	// https://github.com/jtkukunas/zlib
	// https://github.com/cloudflare/zlib

	if (deflateReset(z)) {
		return;
	}

	z->next_in = buffer_;
	z->avail_in = SECTOR_SIZE;

	uint8_t *result = pool.Alloc();

	z->next_out = result;
	z->avail_out = pool.BUFFER_SIZE;

	int res;
	while ((res = deflate(z, Z_FINISH)) == Z_OK) {
		continue;
	}
	if (res == Z_STREAM_END) {
		// Success.  Let's check the size.
		SubmitTrial(result, z->total_out);
	} else {
		// Failed, just ignore this result.
		// TODO: Log or something?
		pool.Release(result);
	}
}

// Frees result if it's not better (takes ownership.)
bool Sector::SubmitTrial(uint8_t *result, uint32_t size) {
	if (size < bestSize_) {
		bestSize_ = size;
		if (best_) {
			pool.Release(best_);
		}
		best_ = result;
		return true;
	} else {
		pool.Release(result);
		return false;
	}
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
