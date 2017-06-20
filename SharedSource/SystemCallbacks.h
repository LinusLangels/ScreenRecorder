#pragma once

#ifdef _WIN32
typedef void(__stdcall *LogCallback)(const char* message);
#else
typedef void(__cdecl *LogCallback)(const char* message);
#endif
