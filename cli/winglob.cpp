#ifdef _WIN32
#include <string>
#include <vector>
#include <windows.h>
#include "winargs.h"
#include "winglob.h"

static bool is_unc(const std::wstring &argw) {
	if (argw.length() > 4) {
		return argw[0] == '\\' && argw[1] == '\\' && argw[2] == '?' && argw[3] == '\\';
	}
	return false;
}

// Translate a path into components, i.e. { "C:", "Users", "Me", "Doc*", "*.foo" }
static std::vector<std::wstring> split_components(const std::wstring &argw) {
	std::vector<std::wstring> components;
	if (is_unc(argw)) {
		// This tells us specifically not to perform wildcard expansion.
		components.push_back(argw);
		return components;
	}

	size_t pos = 0;
	while (pos != argw.npos) {
		size_t next = argw.find_first_of(L"/\\", pos);
		if (next == pos) {
			if (pos != 0) {
				// Ignore doubled up slashes, i.e. C:\\Users\\\\Me.
				++pos;
				continue;
			}
			next = argw.find_first_of(L"/\\", pos + 1);
		}

		if (next == argw.npos) {
			components.push_back(argw.substr(pos));
		} else {
			components.push_back(argw.substr(pos, next - pos));
		}
		pos = next;
	}

	return components;
}

static std::wstring find_base_context(std::vector<std::wstring> &components) {
	if (components[0].length() == 2 && components[0][1] == ':') {
		// Is the first thing a drive letter?
		std::wstring context = components[0] + L"\\";
		components.erase(components.begin());
		return context;
	} else if (components[0].length() == 0) {
		components.erase(components.begin());
		return L"\\";
	} else if (components.size() == 1 && is_unc(components[0])) {
		// UNC path, use it as a context directly to avoid any wildcard parsing.
		std::wstring context = components[0];
		components.clear();
		return context;
	}

	return L"";
}

// Expand the wildcard component against each context folder in input, and return the full list.
static std::vector<std::wstring> next_contexts(const std::vector<std::wstring> &input, std::wstring component) {
	std::vector<std::wstring> output;

	// Special case for . or .. as the component.
	if (component == L"." || component == L"..") {
		for (const std::wstring &context : input) {
			std::wstring base = context.empty() ? L"" : (context + L"\\");
			output.push_back(base + component);
		}
		return output;
	}

	for (const std::wstring &context : input) {
		std::wstring base = context.empty() ? L"" : (context + L"\\");
		std::wstring search = base + component;

		WIN32_FIND_DATAW data;
		HANDLE finder = FindFirstFileW(search.c_str(), &data);
		if (finder == INVALID_HANDLE_VALUE) {
			output.push_back(search);
			continue;
		}
		do {
			std::wstring filename(data.cFileName, wcsnlen(data.cFileName, MAX_PATH));
			if (filename == L"." || filename == L"..") {
				continue;
			}

			output.push_back(base + filename);
		} while (FindNextFileW(finder, &data) != 0);
		FindClose(finder);
	}

	return output;
}

void winargs_get_wildcard(const char *arg, std::vector<std::string> &files) {
	std::wstring argw;
	str_to_utf16z(arg, argw);
	if (argw.empty()) {
		return;
	}
	argw.resize(argw.size() - 1);

	// We need to process each component, one at a time.
	std::vector<std::wstring> components = split_components(argw);
	std::vector<std::wstring> contexts;
	contexts.push_back(find_base_context(components));

	for (const std::wstring &component : components) {
		contexts = next_contexts(contexts, component);
	}

	// Now we have all the expanded files, so convert to utf8.
	for (const std::wstring &wfilename : contexts) {
		std::string filename;
		str_to_utf8z(wfilename.c_str(), filename);
		// Now truncate off the null - we don't need it here.
		if (!filename.empty()) {
			filename.resize(filename.size() - 1);
		}
		files.push_back(filename);
	}
}
#endif
