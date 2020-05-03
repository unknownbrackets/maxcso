#include <cstring>
#include "sector.h"
#include "compress.h"
#include "cso.h"
#include "buffer_pool.h"
#include "zopfli/zopfli.h"
#ifndef NO_DEFLATE7Z
#include "deflate7z.h"
#endif
#include "lz4.h"
#include "lz4hc.h"
#define ZLIB_CONST
#include "zlib.h"

namespace maxcso {

static bool AddZlib(std::vector<z_stream *> &list, int strategy, bool withHeader) {
	z_stream *z = reinterpret_cast<z_stream *>(calloc(1, sizeof(z_stream)));
	int result = deflateInit2(z, 9, Z_DEFLATED, withHeader ? 15 : -15, 9, strategy);
	if (result != Z_OK) {
		free(z);
		return false;
	}

	list.push_back(z);
	return true;
}

static void EndZlib(z_stream *&z) {
	deflateEnd(z);
	free(z);
	z = nullptr;
}

Sector::Sector(uint32_t flags)
	: flags_(flags), origMaxCost_(0), lz4MaxCost_(0), busy_(false), enqueued_(false),
	compress_(true), readySize_(0), buffer_(nullptr), best_(nullptr) {
	// Set up the zlib streams, which we will reuse each time we hit this sector.
	bool withHeader = (flags_ & TASKFLAG_FMT_DAX) != 0;
	if (!(flags_ & TASKFLAG_NO_ZLIB_DEFAULT)) {
		AddZlib(zStreams_, Z_DEFAULT_STRATEGY, withHeader);
	}
	if (!(flags_ & TASKFLAG_NO_ZLIB_BRUTE)) {
		AddZlib(zStreams_, Z_FILTERED, withHeader);
		AddZlib(zStreams_, Z_HUFFMAN_ONLY, withHeader);
		AddZlib(zStreams_, Z_RLE, withHeader);
	}

#ifndef NO_DEFLATE7Z
	if (!(flags_ & TASKFLAG_NO_7ZIP)) {
		Deflate7z::Options opts;
		Deflate7z::SetDefaults(&opts);
		opts.level = 9;
		opts.passes = 12;
		opts.fastbytes = 64;
		opts.matchcycles = 32;
		opts.algo = 1;
		opts.useZlib = (flags_ & TASKFLAG_FMT_DAX) != 0;
		Deflate7z::Alloc(&deflate7z_, &opts);
	}
#endif
}

Sector::~Sector() {
	// Maybe should throw an error if it wasn't released?
	Release();

	for (z_stream *&z : zStreams_) {
		EndZlib(z);
	}

#ifndef NO_DEFLATE7Z
	if (!(flags_ & TASKFLAG_NO_7ZIP)) {
		Deflate7z::Release(&deflate7z_);
	}
#endif
}

void Sector::Process(int64_t pos, uint8_t *buffer, SectorCallback ready) {
	if (!busy_) {
		busy_ = true;

		pos_ = pos & ~static_cast<uint64_t>(blockSize_ - 1);
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

	if (enqueued_) {
		ready_(false, "Sector already waiting for queued operation");
		return;
	}

	if (compress_) {
		enqueued_ = true;
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
		// TODO: For DAX, we allow this, since we don't support NC areas yet.
		if (!(flags_ & TASKFLAG_FMT_DAX)) {
			pool.Release(best_);
			best_ = nullptr;
			bestSize_ = blockSize_;
			bestFmt_ = SECTOR_FMT_ORIG;
		}
	}
}

void Sector::Compress() {
	// Each of these sometimes wins on certain blocks.
	for (z_stream *const z : zStreams_) {
		ZlibTrial(z);
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
	// Try TOO_FAR?  Trialing 3 different values gives ~0.0002% and requires zlib patching...
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
	ZopfliFormat fmt = (flags_ & TASKFLAG_FMT_DAX) != 0 ? ZOPFLI_FORMAT_ZLIB : ZOPFLI_FORMAT_DEFLATE;
	ZopfliCompress(&opt, fmt, buffer_, blockSize_, &out, &outsize);
	if (out != nullptr) {
		if (outsize > 0 && outsize < static_cast<size_t>(pool.bufferSize)) {
			// So that we have proper release semantics, we copy to our buffer.
			uint8_t *result = pool.Alloc();
			memcpy(result, out, outsize);
			SubmitTrial(result, static_cast<uint32_t>(outsize), SECTOR_FMT_DEFLATE);
		}
		free(out);
	}
}

void Sector::SevenZipTrial() {
#ifndef NO_DEFLATE7Z
	uint8_t *result = pool.Alloc();
	uint32_t resultSize = 0;
	if (Deflate7z::Deflate(deflate7z_, result, pool.bufferSize, buffer_, blockSize_, &resultSize)) {
		SubmitTrial(result, resultSize, SECTOR_FMT_DEFLATE);
	} else {
		pool.Release(result);
	}
#endif
}

void Sector::LZ4HCTrial(bool allowBrute) {
	// Sometimes lower levels can actually win.  But, usually not, so only try a few.
	int level = allowBrute ? 4 : 16;
	for (; level <= 16; level += 3) {
		uint8_t *result = pool.Alloc();
		uint32_t resultSize = LZ4_compress_HC(reinterpret_cast<const char *>(buffer_), reinterpret_cast<char *>(result), blockSize_, pool.bufferSize, level);
		if (resultSize != 0) {
			SubmitTrial(result, resultSize, SECTOR_FMT_LZ4);
		} else {
			pool.Release(result);
		}
	}
}

void Sector::LZ4Trial() {
	uint8_t *result = pool.Alloc();
	uint32_t resultSize = LZ4_compress_default(reinterpret_cast<const char *>(buffer_), reinterpret_cast<char *>(result), blockSize_, pool.bufferSize);
	if (resultSize != 0) {
		SubmitTrial(result, resultSize, SECTOR_FMT_LZ4);
	} else {
		pool.Release(result);
	}
}

// Frees result if it's not better (takes ownership.)
bool Sector::SubmitTrial(uint8_t *result, uint32_t size, SectorFormat fmt) {
	bool better = size + origMaxCost_ < bestSize_;

	if (flags_ & TASKFLAG_FMT_DAX) {
		// TODO: Until we support NC areas (but that means we have to rebuild output or compress all blocks first?)
		if (!better && bestFmt_ == SECTOR_FMT_ORIG) {
			better = true;
		}
	}

	// Based on the old and new format, we may want to apply some fuzzing for lz4.
	if (fmt == SECTOR_FMT_LZ4 && bestFmt_ == SECTOR_FMT_DEFLATE) {
		// Allow lz4 to make it larger by a max cost.
		// Also, use lz4 if it's the same size, since it decompresses faster.
		better = size <= bestSize_ + lz4MaxCost_;
	} else if (fmt == SECTOR_FMT_DEFLATE && bestFmt_ == SECTOR_FMT_LZ4) {
		// Reverse of the above.
		better = size + lz4MaxCost_ < bestSize_;
	}

	if (better) {
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
	enqueued_ = false;
	compress_ = true;
	readySize_ = 0;
}

};
