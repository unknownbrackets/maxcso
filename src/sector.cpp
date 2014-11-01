#include "sector.h"
#include "compress.h"
#include "cso.h"
#include "buffer_pool.h"
#include "zopfli/zopfli.h"
#include "deflate7z.h"
#include "lz4.h"
#include "lz4hc.h"
#define ZLIB_CONST
#include "zlib.h"

namespace maxcso {

static int InitZlib(z_stream *&z, int strategy) {
	z = reinterpret_cast<z_stream *>(calloc(1, sizeof(z_stream)));
	return deflateInit2(z, 9, Z_DEFLATED, -15, 9, strategy);
}

static void EndZlib(z_stream *&z) {
	deflateEnd(z);
	free(z);
	z = nullptr;
}

Sector::Sector(uint32_t flags) : flags_(flags), busy_(false), compress_(true), readySize_(0), buffer_(nullptr), best_(nullptr) {
	// Set up the zlib streams, which we will reuse each time we hit this sector.
	if (!(flags_ & TASKFLAG_NO_ZLIB_DEFAULT)) {
		InitZlib(zDefault_, Z_DEFAULT_STRATEGY);
	}
	if (!(flags_ & TASKFLAG_NO_ZLIB_BRUTE)) {
		InitZlib(zFiltered_, Z_FILTERED);
		InitZlib(zHuffman_, Z_HUFFMAN_ONLY);
		InitZlib(zRLE_, Z_RLE);
	}

	if (!(flags_ & TASKFLAG_NO_7ZIP)) {
		Deflate7z::Options opts;
		Deflate7z::SetDefaults(&opts);
		opts.level = 9;
		Deflate7z::Alloc(&deflate7z_, &opts);
	}
}

Sector::~Sector() {
	// Maybe should throw an error if it wasn't released?
	Release();

	if (!(flags_ & TASKFLAG_NO_ZLIB_DEFAULT)) {
		EndZlib(zDefault_);
	}
	if (!(flags_ & TASKFLAG_NO_ZLIB_BRUTE)) {
		EndZlib(zFiltered_);
		EndZlib(zHuffman_);
		EndZlib(zRLE_);
	}

	if (!(flags_ & TASKFLAG_NO_7ZIP)) {
		Deflate7z::Release(&deflate7z_);
	}
}

void Sector::Process(int64_t pos, uint8_t *buffer, SectorCallback ready) {
	if (!busy_) {
		busy_ = true;

		pos_ = pos & ~(blockSize_ - 1);
		bestSize_ = blockSize_;
		bestFmt_ = SECTOR_FMT_ORIG;

		if (blockSize_ == SECTOR_SIZE) {
			buffer_ = buffer;
		} else {
			buffer_ = pool.Alloc();
			memcpy(buffer_ + pos - pos_, buffer, SECTOR_SIZE);
			pool.Release(buffer);
		}
	} else if (static_cast<uint64_t>(pos - pos_) < pool.bufferSize) {
		memcpy(buffer_ + pos - pos_, buffer, SECTOR_SIZE);
		pool.Release(buffer);
	} else {
		ready_(false, "Invalid buffer pos for this sector block");
		return;
	}

	readySize_ += SECTOR_SIZE;
	if (readySize_ < blockSize_) {
		// We can't process yet, wait for the other buffers.
		return;
	}

	if (compress_) {
		ready_ = ready;
		uv_.queue_work(loop_, &work_, [this](uv_work_t *req) {
			Compress();
			FinalizeBest(align_);
		}, [this](uv_work_t *req, int status) {
			if (status < 0) {
				ready_(false, "Failed to compress sector");
			} else {
				ready_(true, nullptr);
			}
		});
	} else {
		ready(true, nullptr);
	}
}

void Sector::FinalizeBest(uint32_t align) {
	// If bestSize_ wouldn't be smaller after alignment, we should not compress.
	// It won't save space, and it'll waste CPU on the decompression side.
	if (AlignedBestSize(align) >= blockSize_ && best_ != nullptr) {
		pool.Release(best_);
		best_ = nullptr;
		bestSize_ = blockSize_;
		bestFmt_ = SECTOR_FMT_ORIG;
	}
}

void Sector::Compress() {
	if (!(flags_ & TASKFLAG_NO_ZLIB_DEFAULT)) {
		ZlibTrial(zDefault_);
	}
	// Each of these sometimes wins on certain blocks.
	if (!(flags_ & TASKFLAG_NO_ZLIB_BRUTE)) {
		ZlibTrial(zFiltered_);
		ZlibTrial(zHuffman_);
		ZlibTrial(zRLE_);
	}
	if (!(flags_ & TASKFLAG_NO_ZOPFLI)) {
		ZopfliTrial();
	}
	if (!(flags_ & TASKFLAG_NO_7ZIP)) {
		SevenZipTrial();
	}
	if (!(flags_ & (TASKFLAG_NO_LZ4_HC | TASKFLAG_NO_LZ4_HC_BRUTE))) {
		LZ4HCTrial(!(flags_ & TASKFLAG_NO_LZ4_HC_BRUTE));
	}
	if (!(flags_ & TASKFLAG_NO_LZ4_DEFAULT)) {
		LZ4Trial();
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
	z->avail_in = blockSize_;

	uint8_t *result = pool.Alloc();

	z->next_out = result;
	z->avail_out = pool.bufferSize;

	int res;
	while ((res = deflate(z, Z_FINISH)) == Z_OK) {
		continue;
	}
	if (res == Z_STREAM_END) {
		// Success.  Let's check the size.
		SubmitTrial(result, z->total_out, SECTOR_FMT_DEFLATE);
	} else {
		// Failed, just ignore this result.
		// TODO: Log or something?
		pool.Release(result);
	}
}

void Sector::ZopfliTrial() {
	// TODO: Trial blocksplittinglast and blocksplittingmax?
	// Increase numiterations depending on how long it takes?
	// TODO: Should this be static otherwise?
	ZopfliOptions opt;
	ZopfliInitOptions(&opt);
	opt.blocksplittinglast = 1;
	opt.numiterations = 5;

	// Grr, zopfli doesn't allow us to use a fixed-size buffer.
	// Also doesn't return failure?
	unsigned char *out = nullptr;
	size_t outsize = 0;
	ZopfliCompress(&opt, ZOPFLI_FORMAT_DEFLATE, buffer_, blockSize_, &out, &outsize);
	if (out != nullptr) {
		if (outsize > 0 && outsize < static_cast<size_t>(bestSize_)) {
			// So that we have proper release semantics, we copy to our buffer.
			uint8_t *result = pool.Alloc();
			memcpy(result, out, outsize);
			SubmitTrial(result, static_cast<uint32_t>(outsize), SECTOR_FMT_DEFLATE);
		}
		free(out);
	}
}

void Sector::SevenZipTrial() {
	uint8_t *result = pool.Alloc();
	uint32_t resultSize = 0;
	if (Deflate7z::Deflate(deflate7z_, result, pool.bufferSize, buffer_, blockSize_, &resultSize)) {
		SubmitTrial(result, resultSize, SECTOR_FMT_DEFLATE);
	} else {
		pool.Release(result);
	}
}

void Sector::LZ4HCTrial(bool allowBrute) {
	// Sometimes lower levels can actually win.  But, usually not, so only try a few.
	int level = allowBrute ? 4 : 16;
	for (; level <= 16; level += 3) {
		uint8_t *result = pool.Alloc();
		uint32_t resultSize = LZ4_compressHC2(reinterpret_cast<const char *>(buffer_), reinterpret_cast<char *>(result), blockSize_, level);
		if (resultSize != 0) {
			SubmitTrial(result, resultSize, SECTOR_FMT_LZ4);
		} else {
			pool.Release(result);
		}
	}
}

void Sector::LZ4Trial() {
	uint8_t *result = pool.Alloc();
	uint32_t resultSize = LZ4_compress(reinterpret_cast<const char *>(buffer_), reinterpret_cast<char *>(result), blockSize_);
	if (resultSize != 0) {
		SubmitTrial(result, resultSize, SECTOR_FMT_LZ4);
	} else {
		pool.Release(result);
	}
}

// Frees result if it's not better (takes ownership.)
bool Sector::SubmitTrial(uint8_t *result, uint32_t size, SectorFormat fmt) {
	if (size < bestSize_) {
		bestSize_ = size;
		if (best_) {
			pool.Release(best_);
		}
		best_ = result;
		bestFmt_ = fmt;
		return true;
	} else {
		pool.Release(result);
		return false;
	}
}

void Sector::Release() {
	if (best_ != nullptr) {
		pool.Release(best_);
		best_ = nullptr;
	}
	if (buffer_ != nullptr) {
		pool.Release(buffer_);
		buffer_ = nullptr;
	}

	busy_ = false;
	compress_ = true;
	readySize_ = 0;
}

};
