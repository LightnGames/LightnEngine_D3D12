#include "include/Utility.h"

WString convertWString(const String& srcStr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t, MyAllocator<wchar_t>> cv;

	//stringÅ®wstring UTF-8 Only No Include Japanese
	return cv.from_bytes(srcStr.c_str());
}