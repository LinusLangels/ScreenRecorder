#include "TimeTools.h"
#include <Windows.h>
#include <sys/types.h>

int64_t timenow_ms()
{
	LARGE_INTEGER cTime;
	FILETIME fTime;
	SYSTEMTIME currentTime;
	GetSystemTime(&currentTime);

	SystemTimeToFileTime(&currentTime, &fTime);
	cTime.HighPart = fTime.dwHighDateTime;
	cTime.LowPart = fTime.dwLowDateTime;

	return cTime.QuadPart / 10000;
}
