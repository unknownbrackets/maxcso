#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
void winargs_get_wildcard(const char *arg, std::vector<std::string> &files);
#endif
