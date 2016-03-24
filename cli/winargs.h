#pragma once

#include <string>

#ifdef _WIN32
void str_to_utf8z(const wchar_t *argw, std::string &arg);
void str_to_utf16z(const char *arg, std::wstring &argw);

char **winargs_get_utf8(int &argc);
#endif
