#pragma once

#include <cstdint>

namespace Deflate7z {

	struct Options {
		uint32_t level;
		uint32_t passes;
		uint32_t fastbytes;
		uint32_t algo;
		uint32_t matchcycles;
		bool useZlib;
	};

	struct Context;

	void SetDefaults(Options *opts);
	bool Deflate(const Options *opts, void *dst, uint32_t dstSize, const void *src, uint32_t srcSize, uint32_t *resultSize);

	// For repeated use.
	void Alloc(Context **ctx, const Options *opts);
	bool Deflate(Context *ctx, void *dst, uint32_t dstSize, const void *src, uint32_t srcSize, uint32_t *resultSize);
	void Release(Context **ctx);

};
