#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace maxcso {

struct Task;

enum TaskStatus {
	TASK_INPROGRESS,
	TASK_SUCCESS,
	TASK_BAD_INPUT,
	TASK_BAD_OUTPUT,
	TASK_INVALID_DATA,
	TASK_CANNOT_WRITE,
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
};

typedef std::function<void (const Task *, TaskStatus status, float completion)> Callback;

struct Task {
	std::string input;
	std::string output;
	Callback progress;
	uint32_t flags;
};

void Compress(const std::vector<Task> &tasks);

};
