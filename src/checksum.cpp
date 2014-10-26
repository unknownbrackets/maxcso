#include <functional>
#include <fcntl.h>
#include <map>
#include "checksum.h"
#include "uv_helper.h"
#include "input.h"
#include "buffer_pool.h"
#include "cso.h"
#define ZLIB_CONST
#include "zlib.h"

namespace maxcso {

class ChecksumTask {
public:
	ChecksumTask(uv_loop_t *loop, const Task &t)
		: task_(t), loop_(loop), input_(-1), inputHandler_(loop) {
	}
	~ChecksumTask() {
		Cleanup();
	}

	void Enqueue();
	void Cleanup();

private:
	void HandleBuffer(uint8_t *buffer);

	void Notify(TaskStatus status, int64_t pos = -1, int64_t total = -1, int64_t written = -1) {
		if (status == TASK_INPROGRESS || status == TASK_SUCCESS) {
			task_.progress(&task_, status, pos, total, written);
		} else {
			task_.error(&task_, status, nullptr);
		}
	}
	void Notify(TaskStatus status, const char *reason) {
		task_.error(&task_, status, reason);
	}

	void BeginProcessing();

	UVHelper uv_;
	const Task &task_;
	uv_loop_t *loop_;

	uv_fs_t read_;

	uv_file input_;
	Input inputHandler_;

	int64_t pos_;
	int64_t size_;
	uint32_t crc_;
	std::map<int64_t, uint8_t *> pendingBuffers_;
};

void ChecksumTask::Enqueue() {
	// We open input and output in order in case there are errors.
	uv_.fs_open(loop_, &read_, task_.input.c_str(), O_RDONLY, 0444, [this](uv_fs_t *req) {
		if (req->result < 0) {
			Notify(TASK_BAD_INPUT, "Could not open input file");
		} else {
			input_ = static_cast<uv_file>(req->result);

			BeginProcessing();
		}
		uv_fs_req_cleanup(req);
	});
}

void ChecksumTask::Cleanup() {
	if (input_ >= 0) {
		uv_fs_close(loop_, &read_, input_, nullptr);
		uv_fs_req_cleanup(&read_);
		input_ = -1;
	}
	for (auto pair : pendingBuffers_) {
		pool.Release(pair.second);
	}
	pendingBuffers_.clear();
}

void ChecksumTask::BeginProcessing() {
	pos_ = 0;

	inputHandler_.OnFinish([this](bool success, const char *reason) {
		if (!success) {
			Notify(TASK_INVALID_DATA, reason);
		}
	});

	inputHandler_.OnBegin([this](int64_t size) {
		crc_ = crc32(0L, Z_NULL, 0);
		size_ = size;
		Notify(TASK_INPROGRESS, 0, size, 0);
	});
	inputHandler_.Pipe(input_, [this](int64_t pos, uint8_t *buffer) {
		// In case we allow the buffers to come out of order, let's use a queue.
		if (pos_ == pos) {
			HandleBuffer(buffer);
		} else {
			pendingBuffers_[pos] = buffer;
		}
	});
}

void ChecksumTask::HandleBuffer(uint8_t *buffer) {
	// We're doing this on the main thread, but it should be okay.
	crc_ = crc32(crc_, buffer, SECTOR_SIZE);
	pool.Release(buffer);
	pos_ += SECTOR_SIZE;

	// Flush any in the queue that we can now use.
	if (!pendingBuffers_.empty()) {
		auto it = pendingBuffers_.begin();
		auto begin = it;
		for (; it != pendingBuffers_.end(); ++it) {
			if (it->first != pos_) {
				break;
			}

			crc_ = crc32(crc_, it->second, SECTOR_SIZE);
			pool.Release(it->second);
			pos_ += SECTOR_SIZE;
		}

		// If we used any, erase them.
		if (begin != it) {
			pendingBuffers_.erase(begin, it);
		}
	}

	Notify(TASK_INPROGRESS, pos_, size_, 0);

	if (pos_ == size_) {
		char temp[128];
		sprintf(temp, "CRC32: %08x", crc_);
		Notify(TASK_SUCCESS, temp);
	}
}

void Checksum(const std::vector<Task> &tasks) {
	uv_loop_t loop;
	uv_loop_init(&loop);

	for (const Task t : tasks) {
		ChecksumTask task(&loop, t);
		task.Enqueue();
		uv_run(&loop, UV_RUN_DEFAULT);
	}

	// Run any remaining events from destructors.
	uv_run(&loop, UV_RUN_DEFAULT);

	uv_loop_close(&loop);
}

};
