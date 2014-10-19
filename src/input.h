#pragma once

#include <string>
#include "uv_helper.h"

namespace maxcso {

typedef std::function<void (int64_t pos, uint8_t *sector)> InputCallback;
typedef std::function<void (bool success, const char *reason)> InputFinishCallback;

class Input {
public:
	Input(uv_loop_t *loop);
	~Input();
	void OnFinish(InputFinishCallback finish);
	void Pipe(uv_file file, InputCallback callback);

	int64_t TotalSize() {
		return size_;
	}

private:
	void DetectFormat();
	void ReadSector();
	bool DecompressSector(uint8_t *dst, const uint8_t *src, unsigned int len, std::string &err);

	enum FileType {
		UNKNOWN,
		ISO,
		CISO,
	};

	UVHelper uv_;
	uv_loop_t *loop_;

	InputFinishCallback finish_;
	InputCallback callback_;
	uv_file file_;
	uv_fs_t req_;
	uv_work_t work_;
	FileType type_;

	int64_t pos_;
	int64_t size_;

	uint8_t csoShift_;
	// TODO: Endian?
	uint32_t *csoIndex_;
	std::string csoError_;
};

};
