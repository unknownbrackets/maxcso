#ifndef _WIN32
#error Should only be compiled on Windows.
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <vector>
#include <cstring>
#include <Windows.h>
#include <ShellAPI.h>
#include "winargs.h"

static std::vector<std::string> args;
static std::vector<char *> arg_pointers;

// Note: includes the null terminator.
void str_to_utf8z(const wchar_t *argw, std::string &arg) {
	int bytes = WideCharToMultiByte(CP_UTF8, 0, argw, -1, nullptr, 0, nullptr, nullptr);

	arg.resize(bytes);
	WideCharToMultiByte(CP_UTF8, 0, argw, -1, &arg[0], bytes, nullptr, nullptr);
}

// Note: includes the null terminator.
void str_to_utf16z(const char *arg, std::wstring &argw) {
	int chars = MultiByteToWideChar(CP_UTF8, 0, arg, -1, nullptr, 0);

	argw.resize(chars);
	MultiByteToWideChar(CP_UTF8, 0, arg, -1, &argw[0], chars);
}

char **winargs_get_utf8(int &argc) {
	wchar_t *cmdLine = GetCommandLineW();
	wchar_t **argvw = CommandLineToArgvW(cmdLine, &argc);

	// There's only one command line, so calculate it here.
	args.resize(argc);
	arg_pointers.resize(argc);

	for (int i = 0; i < argc; ++i) {
		str_to_utf8z(argvw[i], args[i]);
		// Note: already has a null terminator.
		arg_pointers[i] = &args[i][0];
	}

	return &arg_pointers[0];
}
