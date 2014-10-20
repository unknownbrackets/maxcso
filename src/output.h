#pragma once

#include <functional>
#include "uv_helper.h"

namespace maxcso {

typedef std::function<void (float progress)> OutputCallback;
typedef std::function<void (bool status, const char *reason)> OutputFinishCallback;

class Output {
public:
	Output(uv_loop_t *loop);
	~Output();

	void SetFile(uv_file file, int64_t srcSize);
	void Enqueue(int64_t pos, uint8_t *sector);
	void Flush(); // callback when done?
	bool QueueFull();

	void OnProgress(OutputCallback callback);
	void OnFinish(OutputFinishCallback callback);

private:
	void Align();

	UVHelper uv_;
	uv_loop_t *loop_;

	uv_file file_;
	uv_fs_t req_;
	uv_fs_t flush_;

	int64_t srcSize_;
	int64_t srcPos_;
	int64_t dstPos_;

	uint32_t *index_;
	uint8_t shift_;
	uint32_t align_;

	OutputCallback progress_;
	OutputFinishCallback finish_;

	bool writing_;
	// TODO: Queue, vector of free ones, map of pending?
};

};
