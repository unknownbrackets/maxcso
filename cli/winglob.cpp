#ifndef _WIN32
#error Should only be compiled on Windows.
#endif

#define _WIN32_WINNT 0x501
#include <string>
#include <afx.h>
#include "winargs.h"
#include "winglob.h"

void winargs_get_wildcard(const char *arg, std::vector<std::string> &files) {
	std::wstring argw;
	str_to_utf16(arg, argw);

	CFileFind finder;
	if (!finder.FindFile(argw.c_str())) {
		// It wasn't found.  Add the original arg for sane error handling.
		files.push_back(arg);
	} else {
		BOOL hasMore = TRUE;
		while (hasMore) {
			// Confusingly, FindNextFile returns false on the last file.
			hasMore = finder.FindNextFile();
			if (finder.IsDots()) {
				continue;
			}

			std::string filename;
			str_to_utf8(finder.GetFilePath().GetString(), filename);
			files.push_back(filename);
		}
	}
	finder.Close();
}
