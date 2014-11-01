#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace maxcso {

static const char *VERSION = "1.3.0";

struct Task;

enum TaskStatus {
	TASK_INPROGRESS,
	TASK_SUCCESS,
	TASK_BAD_INPUT,
	TASK_BAD_OUTPUT,
	TASK_INVALID_DATA,
	TASK_CANNOT_WRITE,
	TASK_INVALID_OPTION,
};

enum TaskFlags {
	TASKFLAG_DEFAULT = 0,

	// Disable certain compression algorithms.
	TASKFLAG_NO_ZLIB = 0x03,
	TASKFLAG_NO_ZLIB_DEFAULT = 0x01,
	TASKFLAG_NO_ZLIB_BRUTE = 0x02,
	TASKFLAG_NO_ZOPFLI = 0x04,
	TASKFLAG_NO_7ZIP = 0x08,

	// Disable heuristics and compress all sectors.
	TASKFLAG_FORCE_ALL = 0x10,

	// Flags for new formats.
	TASKFLAG_FMT_ZSO = 0x20,
	TASKFLAG_FMT_CSO_2 = 0x40,
	TASKFLAG_NO_LZ4 = 0x380,
	TASKFLAG_NO_LZ4_DEFAULT = 0x80,
	TASKFLAG_NO_LZ4_HC = 0x100,
	TASKFLAG_NO_LZ4_HC_BRUTE = 0x200,

	TASKFLAG_NO_ALL = TASKFLAG_NO_ZLIB | TASKFLAG_NO_ZOPFLI | TASKFLAG_NO_7ZIP | TASKFLAG_NO_LZ4,
};

typedef std::function<void (const Task *, TaskStatus status, int64_t pos, int64_t total, int64_t written)> ProgressCallback;
typedef std::function<void (const Task *, TaskStatus status, const char *reason)> ErrorCallback;

struct Task {
	std::string input;
	std::string output;
	ProgressCallback progress;
	ErrorCallback error;
	uint32_t block_size;
	uint32_t flags;
};

void Compress(const std::vector<Task> &tasks);

};
