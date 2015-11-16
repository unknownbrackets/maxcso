#include <functional>
#include <fcntl.h>
#include "compress.h"
#include "uv_helper.h"
#include "cso.h"
#include "input.h"
#include "output.h"
#include "buffer_pool.h"

namespace maxcso {

// Anything above this is insane.  This value is even insane.
static const uint32_t MAX_BLOCK_SIZE = 0x40000;

// These are the default block sizes.
static const uint32_t SMALL_BLOCK_SIZE = 2048;
static const uint32_t LARGE_BLOCK_SIZE = 16384;
// We use the LARGE_BLOCK_SIZE default for files larger than 2GB.
static const int64_t LARGE_BLOCK_SIZE_THRESH = 0x80000000;

// This actually handles decompression too.  They're basically the same.
class CompressionTask {
public:
	CompressionTask(uv_loop_t *loop, const Task &t)
		: task_(t), loop_(loop), input_(-1), inputHandler_(loop), outputHandler_(loop, t), output_(-1),
		blockSize_(0) {
	}
	~CompressionTask() {
		Cleanup();
	}

	void Enqueue();
	void Cleanup();

private:
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
	uv_fs_t write_;

	uv_file input_;
	Input inputHandler_;
	Output outputHandler_;
	uv_file output_;
	uint32_t blockSize_;
};

void CompressionTask::Enqueue() {
	if (task_.block_size == DEFAULT_BLOCK_SIZE) {
		// Start with a small block size.
		// We'll re-evaluate later.
		blockSize_ = SMALL_BLOCK_SIZE;
	} else {
		if (task_.block_size > MAX_BLOCK_SIZE) {
			Notify(TASK_INVALID_OPTION, "Block size too large");
			return;
		}
		if (task_.block_size < SECTOR_SIZE) {
			Notify(TASK_INVALID_OPTION, "Block size too small, must be at least 2048");
			return;
		}
		if ((task_.block_size & (task_.block_size - 1)) != 0) {
			Notify(TASK_INVALID_OPTION, "Block size must be a power of two");
			return;
		}
		blockSize_ = task_.block_size;
	}
	if (!pool.SetBufferSize(blockSize_ * 2)) {
		Notify(TASK_INVALID_OPTION, "Unable to update buffer size to match block size");
		return;
	}

	// We open input and output in order in case there are errors.
	uv_.fs_open(loop_, &read_, task_.input.c_str(), O_RDONLY, 0444, [this](uv_fs_t *req) {
		if (req->result < 0) {
			Notify(TASK_BAD_INPUT, "Could not open input file");
		} else {
			input_ = static_cast<uv_file>(req->result);
			uv_.fs_open(loop_, &write_, task_.output.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644, [this](uv_fs_t *req) {
				if (req->result < 0) {
					Notify(TASK_BAD_OUTPUT, "Could not open output file");
				} else {
					output_ = static_cast<uv_file>(req->result);

					// Okay, both files opened fine, it's time to turn on the tap.
					BeginProcessing();
				}
				uv_fs_req_cleanup(req);
			});
		}
		uv_fs_req_cleanup(req);
	});
}

void CompressionTask::Cleanup() {
	if (input_ >= 0) {
		uv_fs_close(loop_, &read_, input_, nullptr);
		uv_fs_req_cleanup(&read_);
		input_ = -1;
	}
	if (output_ >= 0) {
		uv_fs_close(loop_, &write_, output_, nullptr);
		uv_fs_req_cleanup(&write_);
		output_ = -1;
	}
}

void CompressionTask::BeginProcessing() {
	inputHandler_.OnFinish([this](bool success, const char *reason) {
		if (!success) {
			Notify(TASK_INVALID_DATA, reason);
		}
	});
	outputHandler_.OnFinish([this](bool success, const char *reason) {
		if (success) {
			Notify(TASK_SUCCESS);
		} else {
			// Abort reading.
			inputHandler_.Pause();
			Notify(TASK_CANNOT_WRITE, reason);
		}
	});

	outputHandler_.OnProgress([this](int64_t pos, int64_t total, int64_t written) {
		// If it was paused, the queue has space now.
		inputHandler_.Resume();
		Notify(TASK_INPROGRESS, pos, total, written);
	});

	inputHandler_.OnBegin([this](int64_t size) {
		CSOFormat fmt = CSO_FMT_CSO1;
		if (task_.flags & TASKFLAG_FMT_CSO_2) {
			fmt = CSO_FMT_CSO2;
		} else if (task_.flags & TASKFLAG_FMT_ZSO) {
			fmt = CSO_FMT_ZSO;
		}

		// Now that we know the file size, check if we should resize the blockSize_.
		if (task_.block_size == DEFAULT_BLOCK_SIZE && size >= LARGE_BLOCK_SIZE_THRESH) {
			blockSize_ = LARGE_BLOCK_SIZE;
			if (!pool.SetBufferSize(blockSize_ * 2)) {
				// Abort reading.
				inputHandler_.Pause();
				Notify(TASK_INVALID_OPTION, "Unable to update buffer size to match block size");
				return;
			}
		}

		outputHandler_.SetFile(output_, size, blockSize_, fmt);
		Notify(TASK_INPROGRESS, 0, size, 0);
	});
	inputHandler_.Pipe(input_, [this](int64_t pos, uint8_t *sector) {
		outputHandler_.Enqueue(pos, sector);
		if (outputHandler_.QueueFull()) {
			inputHandler_.Pause();
		}
	});
}

void Compress(const std::vector<Task> &tasks) {
	uv_loop_t loop;
	uv_loop_init(&loop);

	for (const Task t : tasks) {
		CompressionTask task(&loop, t);
		task.Enqueue();
		uv_run(&loop, UV_RUN_DEFAULT);
	}

	// Run any remaining events from destructors.
	uv_run(&loop, UV_RUN_DEFAULT);

	uv_loop_close(&loop);
}

};
