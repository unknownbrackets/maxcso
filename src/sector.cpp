#include "sector.h"
#include "compress.h"
#include "cso.h"
#include "buffer_pool.h"
#include "zopfli/zopfli.h"
#include "deflate7z.h"
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

Sector::Sector(uint32_t flags) : flags_(flags), busy_(false), buffer_(nullptr), best_(nullptr) {
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

void Sector::Process(uv_loop_t *loop, int64_t pos, uint8_t *buffer, uint32_t align, SectorCallback ready) {
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

	uv_.queue_work(loop_, &work_, [this, align](uv_work_t *req) {
		Compress();
		FinalizeBest(align);
	}, [this](uv_work_t *req, int status) {
		if (status < 0) {
			ready_(false, "Failed to compress sector");
		} else {
			ready_(true, nullptr);
		}
	});
}

void Sector::FinalizeBest(uint32_t align) {
	// If bestSize_ wouldn't be smaller after alignment, we should not compress.
	// It won't save space, and it'll waste CPU on the decompression side.
	if (AlignedBestSize(align) >= SECTOR_SIZE) {
		pool.Release(best_);
		best_ = nullptr;
		bestSize_ = SECTOR_SIZE;
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
	ZopfliCompress(&opt, ZOPFLI_FORMAT_DEFLATE, buffer_, SECTOR_SIZE, &out, &outsize);
	if (out != nullptr) {
		if (outsize > 0 && outsize < static_cast<size_t>(bestSize_)) {
			// So that we have proper release semantics, we copy to our buffer.
			uint8_t *result = pool.Alloc();
			memcpy(result, out, outsize);
			SubmitTrial(result, static_cast<uint32_t>(outsize));
		}
		free(out);
	}
}

void Sector::SevenZipTrial() {
	uint8_t *result = pool.Alloc();
	uint32_t resultSize = 0;
	if (Deflate7z::Deflate(deflate7z_, result, pool.BUFFER_SIZE, buffer_, SECTOR_SIZE, &resultSize)) {
		SubmitTrial(result, resultSize);
	} else {
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
	best_ = nullptr;
	bestSize_ = SECTOR_SIZE;
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
