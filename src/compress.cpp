#include <functional>
#include <fcntl.h>
#include "compress.h"
#include "uv_helper.h"
#include "input.h"
#include "buffer_pool.h"

namespace maxcso {

class CompressionTask {
public:
	CompressionTask(uv_loop_t *loop, const Task &t)
		: task_(t), loop_(loop), input_(-1), inputHandler_(loop), output_(-1) {
	}
	~CompressionTask() {
		Cleanup();
	}

	void Enqueue();
	void Cleanup();

private:
	void Notify(TaskStatus status, float progress = 1.0f) {
		task_.progress(&task_, status, progress);
	}

	void BeginProcessing();

	UVHelper uv_;
	const Task &task_;
	uv_loop_t *loop_;

	uv_fs_t read_;
	uv_fs_t write_;

	uv_file input_;
	Input inputHandler_;
	uv_file output_;
};

void CompressionTask::Enqueue() {
	// We open input and output in order in case there are errors.
	uv_.fs_open(loop_, &read_, task_.input.c_str(), O_RDONLY, 0444, [this](uv_fs_t *req) {
		if (req->result < 0) {
			Notify(TASK_BAD_INPUT);
		} else {
			input_ = static_cast<uv_file>(req->result);
			uv_.fs_open(loop_, &write_, task_.output.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644, [this](uv_fs_t *req) {
				if (req->result < 0) {
					Notify(TASK_BAD_OUTPUT);
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
		uv_.fs_close(loop_, &read_, input_, [this](uv_fs_t *req) {
			uv_fs_req_cleanup(req);
		});
		input_ = -1;
	}
	if (output_ >= 0) {
		uv_.fs_close(loop_, &write_, output_, [this](uv_fs_t *req) {
			uv_fs_req_cleanup(req);
		});
		output_ = -1;
	}
}

void CompressionTask::BeginProcessing() {
	inputHandler_.OnFinish([this](bool success, const char *reason) {
		if (success) {
			// TODO: Flush
			Notify(TASK_SUCCESS);
		} else {
			Notify(TASK_INVALID_DATA);
			if (reason) {
				printf("error: %s\n", reason);
			}
		}
	});

	inputHandler_.OnBegin([this](int64_t size) {
		Notify(TASK_INPROGRESS, 0.0f);
	});
	inputHandler_.Pipe(input_, [this](int64_t pos, uint8_t *sector) {
		// TODO
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
