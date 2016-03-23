#pragma once

#include <string>

#ifdef _WIN32
void str_to_utf8(const wchar_t *argw, std::string &arg);
void str_to_utf16(const char *arg, std::wstring &argw);

char **winargs_get_utf8(int &argc);
#endif
