#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "../src/compress.h"
#include "../src/checksum.h"
#include "../libuv/include/uv.h"

void show_version() {
	fprintf(stderr, "maxcso v%s\n", maxcso::VERSION);
}

void show_help(const char *arg0) {
	show_version();
	fprintf(stderr, "Usage: %s [--args] input.iso [-o output.cso]\n", arg0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Multiple files may be specified.  Inputs can be iso or cso files.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --threads=N     Specify N threads for I/O and compression\n");
	fprintf(stderr, "   --quiet         Suppress status output\n");
	fprintf(stderr, "   --crc           Log CRC32 checksums, ignore output files and methods\n");
	fprintf(stderr, "   --fast          Use only basic zlib for fastest result\n");
	fprintf(stderr, "   --block=N       Specify a block size (default is 2048)\n");
	fprintf(stderr, "                   Most readers only support the 2048 size\n");
	// TODO: Bring this back once it's functional.
	//fprintf(stderr, "   --smallest      Force compression of all sectors for smallest result\n");
	fprintf(stderr, "   --use-METHOD    Use a compression method: zlib, Zopfli, or 7zdeflate\n");
	fprintf(stderr, "                   By default, zlib and 7zdeflate are used\n");
	fprintf(stderr, "   --no-METHOD     Disable a compression method, same as above\n");
}

bool has_arg_value(int &i, char *argv[], const std::string &arg, const char *&val) {
	if (arg.compare(0, arg.npos, argv[i], arg.size()) == 0) {
		if (strlen(argv[i]) == arg.size()) {
			val = argv[++i];
			return true;
		} else if (argv[i][arg.size()] == '=') {
			val = argv[i] + arg.size() + 1;
			return true;
		}
	}

	return false;
}

bool has_arg(int &i, char *argv[], const std::string &arg) {
	if (arg.compare(argv[i]) == 0) {
		return true;
	}

	return false;
}

struct Arguments {
	std::vector<std::string> inputs;
	std::vector<std::string> outputs;
	int threads;
	uint32_t block_size;
	uint32_t flags;
	bool quiet;
	bool crc;
};

void default_args(Arguments &args) {
	args.threads = 0;
	args.block_size = 2048;
	args.flags = maxcso::TASKFLAG_NO_ZOPFLI;
	args.quiet = false;
	args.crc = false;
}

int parse_args(Arguments &args, int argc, char *argv[]) {
	const char *val = nullptr;
	int i;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (has_arg(i, argv, "--help") || has_arg(i, argv, "-h")) {
				show_help(argv[0]);
				return 1;
			} else if (has_arg(i, argv, "--version") || has_arg(i, argv, "-v")) {
				show_version();
				return 1;
			} else if (has_arg_value(i, argv, "--block", val)) {
				args.block_size = atoi(val);
			} else if (has_arg_value(i, argv, "--threads", val)) {
				args.threads = atoi(val);
			} else if (has_arg(i, argv, "--crc")) {
				args.crc = true;
			} else if (has_arg(i, argv, "--quiet")) {
				args.quiet = true;
			} else if (has_arg(i, argv, "--fast")) {
				args.flags |= maxcso::TASKFLAG_NO_ZLIB_BRUTE | maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_7ZIP;
			} else if (has_arg(i, argv, "--smallest")) {
				args.flags |= maxcso::TASKFLAG_FORCE_ALL;
			} else if (has_arg(i, argv, "--use-zopfli")) {
				args.flags &= ~maxcso::TASKFLAG_NO_ZOPFLI;
			} else if (has_arg(i, argv, "--use-zlib")) {
				args.flags &= ~maxcso::TASKFLAG_NO_ZLIB;
			} else if (has_arg(i, argv, "--use-7zdeflate")) {
				args.flags &= ~maxcso::TASKFLAG_NO_7ZIP;
			} else if (has_arg(i, argv, "--no-zopfli")) {
				args.flags |= maxcso::TASKFLAG_NO_ZOPFLI;
			} else if (has_arg(i, argv, "--no-zlib")) {
				args.flags |= maxcso::TASKFLAG_NO_ZLIB;
			} else if (has_arg(i, argv, "--no-7zdeflate")) {
				args.flags |= maxcso::TASKFLAG_NO_7ZIP;
			} else if (has_arg_value(i, argv, "--out", val) || has_arg_value(i, argv, "-o", val)) {
				args.outputs.push_back(val);
			} else if (has_arg(i, argv, "--")) {
				break;
			} else {
				show_help(argv[0]);
				fprintf(stderr, "\nERROR: Unknown argument: %s\n", argv[i]);
				return 1;
			}
		} else {
			args.inputs.push_back(argv[i]);
		}
	}

	for (; i < argc; ++i) {
		// If we're here, it means we hit a "--".  The rest are inputs, not args.
		args.inputs.push_back(argv[i]);
	}

	return 0;
}

int validate_args(const char *arg0, Arguments &args) {
	if (args.threads == 0) {
		uv_cpu_info_t *cpus;
		uv_cpu_info(&cpus, &args.threads);
		uv_free_cpu_info(cpus, args.threads);
	}

	if (args.inputs.size() < args.outputs.size()) {
		show_help(arg0);
		fprintf(stderr, "\nERROR: Too many output files.\n");
		return 1;
	}

	if (args.crc) {
		if (args.outputs.size()) {
		show_help(arg0);
		fprintf(stderr, "\nERROR: Output files not used with --crc.\n");
		return 1;
		}
	} else {
		// Automatically write to .cso files if not specified.
		for (size_t i = args.outputs.size(); i < args.inputs.size(); ++i) {
			if (args.inputs[i].size() <= 4) {
				continue;
			}

			const std::string ext = args.inputs[i].substr(args.inputs[i].size() - 4);
			if (ext == ".iso" || ext == ".ISO") {
				args.outputs.push_back(args.inputs[i].substr(0, args.inputs[i].size() - 4) + ".cso");
			}
		}

		if (args.inputs.size() != args.outputs.size()) {
			show_help(arg0);
			fprintf(stderr, "\nERROR: Too few output files.\n");
			return 1;
		}
	}

	if (args.inputs.empty()) {
		show_help(arg0);
		fprintf(stderr, "\nERROR: No input files.\n");
		return 1;
	}

	return 0;
}

#ifdef _WIN32
int setenv(const char *name, const char *value, int) {
	return _putenv_s(name, value);
}
#endif

inline uv_buf_t uv_buf_init(const char *str) {
	return uv_buf_init(const_cast<char *>(str), static_cast<unsigned int>(strlen(str)));
}

inline uv_buf_t uv_buf_init(const std::string &str) {
	return uv_buf_init(const_cast<char *>(str.c_str()), static_cast<unsigned int>(str.size()));
}

const std::string ANSI_RESET_LINE = "\033[2K\033[0G";

int main(int argc, char *argv[]) {
	Arguments args;
	default_args(args);
	int result = parse_args(args, argc, argv);
	if (result != 0) {
		return result;
	}
	result = validate_args(argv[0], args);
	if (result != 0) {
		return result;
	}

	char threadpool_size[32];
	sprintf(threadpool_size, "%d", args.threads);
	setenv("UV_THREADPOOL_SIZE", threadpool_size, 1);

	uv_loop_t loop;
	uv_tty_t tty;
	uv_loop_init(&loop);
	uv_tty_init(&loop, &tty, 2, 0);
	uv_tty_set_mode(&tty, 0);
	bool formatting = uv_guess_handle(2) == UV_TTY && !args.quiet;

	int64_t next = uv_hrtime();
	int64_t lastPos = 0;
	// 50ms
	static const int64_t interval_ns = 50000000LL;
	static const double interval_to_s = 1000.0 / 50.0;

	std::string statusInfo;
	uv_write_t write_req;
	uv_buf_t bufs[2];

	maxcso::ProgressCallback progress = [&] (const maxcso::Task *task, maxcso::TaskStatus status, int64_t pos, int64_t total, int64_t written) {
		if (!formatting) {
			return;
		}

		statusInfo.clear();

		if (status == maxcso::TASK_INPROGRESS) {
			int64_t now = uv_hrtime();
			if (now >= next) {
				double percent = (pos * 100.0) / total;
				double ratio = (written * 100.0) / pos;
				double speed = 0.0;
				int64_t diff = pos - lastPos;
				if (diff > 0) {
					speed = (diff / 1024.0 / 1024.0) * interval_to_s;
				}

				char temp[128];
				sprintf(temp, "%3.0f%%, ratio=%3.0f%%, speed=%5.2f MB/s", percent, ratio, speed);
				statusInfo = temp;

				next = now + interval_ns;
				lastPos = pos;
			}
		} else if (status == maxcso::TASK_SUCCESS) {
			statusInfo = "Complete\n";
		} else {
			// This shouldn't happen.
			statusInfo = "Something went wrong.\n";
		}

		if (statusInfo.empty()) {
			return;
		}

		unsigned int nbufs = 0;
		if (formatting) {
			bufs[nbufs++] = uv_buf_init(ANSI_RESET_LINE);
		}

		if (task->input.size() > 38) {
			statusInfo = "..." + task->input.substr(task->input.size() - 35) + ": " + statusInfo;
		} else {
			statusInfo = task->input + ": " + statusInfo;
		}

		bufs[nbufs++] = uv_buf_init(statusInfo);
		uv_write(&write_req, reinterpret_cast<uv_stream_t *>(&tty), bufs, nbufs, nullptr);
	};
	maxcso::ErrorCallback error = [&] (const maxcso::Task *task, maxcso::TaskStatus status, const char *reason) {
		if (args.quiet) {
			return;
		}

		const std::string prefix = status == maxcso::TASK_SUCCESS ? "" : "Error while processing ";
		statusInfo = (formatting ? ANSI_RESET_LINE : "") + prefix + task->input + ": " + reason + "\n";
		bufs[0] = uv_buf_init(statusInfo);
		uv_write(&write_req, reinterpret_cast<uv_stream_t *>(&tty), bufs, 1, nullptr);
	};

	std::vector<maxcso::Task> tasks;
	for (size_t i = 0; i < args.inputs.size(); ++i) {
		maxcso::Task task;
		task.input = args.inputs[i];
		if (!args.crc) {
			task.output = args.outputs[i];
		}
		task.progress = progress;
		task.error = error;
		task.block_size = args.block_size;
		task.flags = args.flags;
		tasks.push_back(std::move(task));
	}

	if (args.crc) {
		maxcso::Checksum(tasks);
	} else {
		maxcso::Compress(tasks);
	}

	uv_tty_reset_mode();
	uv_loop_close(&loop);
	return 0;
}
