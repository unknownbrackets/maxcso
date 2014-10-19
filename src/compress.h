#pragma once

#include <string>
#include <vector>
#include <functional>

namespace maxcso {

struct Task;

enum TaskStatus {
	TASK_INPROGRESS,
	TASK_SUCCESS,
	TASK_BAD_INPUT,
	TASK_BAD_OUTPUT,
	TASK_INVALID_DATA,
};

typedef std::function<void (const Task *, TaskStatus status, float completion)> Callback;

struct Task {
	std::string input;
	std::string output;
	Callback progress;
};

void Compress(const std::vector<Task> &tasks);

};
