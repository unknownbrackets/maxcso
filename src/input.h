#pragma once

#include <string>
#include "uv_helper.h"

namespace maxcso {

typedef std::function<void (int64_t pos, uint8_t *sector)> InputCallback;
typedef std::function<void (int64_t size)> InputBeginCallback;
typedef std::function<void (bool success, const char *reason)> InputFinishCallback;

class Input {
public:
	Input(uv_loop_t *loop);
	~Input();
	void OnFinish(InputFinishCallback finish);
	void OnBegin(InputBeginCallback begin);
	void Pipe(uv_file file, InputCallback callback);
	void Pause();
	void Resume();

private:
	void DetectFormat();
	void SetupCache(uint32_t minSize);
	void ReadSector();
	void EnqueueDecompressSector(uint8_t *src, uint32_t len, uint32_t offset);

	enum FileType {
		UNKNOWN,
		ISO,
		CISO,
		DAX,
	};

	bool DecompressSector(uint8_t *dst, const uint8_t *src, unsigned int len, FileType type, std::string &err);

	UVHelper uv_;
	uv_loop_t *loop_;

	InputBeginCallback begin_;
	InputFinishCallback finish_;
	InputCallback callback_;
	uv_file file_;
	uv_fs_t req_;
	uv_work_t work_;
	FileType type_;

	bool paused_;
	bool resumeShouldRead_;
	int64_t pos_;
	int64_t size_;
	uint8_t *cache_;
	int64_t cachePos_;
	int32_t cacheSize_;

	std::string decompressError_;
	uint8_t csoIndexShift_;
	uint8_t csoBlockShift_;
	uint32_t csoBlockSize_;
	// TODO: Endian?
	union {
		uint32_t *csoIndex_;
		uint32_t *daxIndex_;
	};
	uint16_t *daxSize_;
	bool *daxIsNC_;
};

};
