#pragma once

// CoreInterface.cpp : Defines the exported functions for the DLL.
//

#include <windows.h>
#include <stdlib.h>
#include "SystemCallbacks.h"

extern "C" {
	#include "libavutil/mathematics.h"
	#include "libavformat/avformat.h"
	#include "libavcodec/avcodec.h"
	#include "libswscale/swscale.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/cpu.h"
	#include "libavutil/mem.h"
	#include "libavutil/samplefmt.h"
	#include "libavcodec/avcodec.h"
	#include "libavutil/frame.h"
}

#ifdef CAPTUREINTERFACE
#define SCREENRECORDER_INTERFACE __declspec(dllexport) 
#else
#define SCREENRECORDER_INTERFACE __declspec(dllimport) 
#endif

extern "C" {
	SCREENRECORDER_INTERFACE void __stdcall SetLogCallback(LogCallback callback);

	SCREENRECORDER_INTERFACE void __stdcall SetDebugPath(const char* path);

	SCREENRECORDER_INTERFACE int __stdcall CreateEncoder(const char* videoPath, int codec, int width, int height, int framerate, int bitrate, int iframeInterval);

	SCREENRECORDER_INTERFACE int __stdcall DestroyEncoder();

	SCREENRECORDER_INTERFACE int __stdcall StartEncoding(int inputWidth, int inputHeight, int outputWidth, int outputHeight, int inputFramerate);

	SCREENRECORDER_INTERFACE int __stdcall StopEncoding();

	SCREENRECORDER_INTERFACE int __stdcall EncodeFrames(int maxFrames);

	SCREENRECORDER_INTERFACE int __stdcall CreateMuxer(const char* videoFile, const char* audioFile);

	SCREENRECORDER_INTERFACE int __stdcall StartMuxing(const char* outputFile);

	SCREENRECORDER_INTERFACE int __stdcall DestroyMuxer();

	SCREENRECORDER_INTERFACE void __stdcall StartCapturing(int _width, int _height);

	SCREENRECORDER_INTERFACE void  __stdcall StopCapturing();

	SCREENRECORDER_INTERFACE int64_t __stdcall GetHighresTime();
}
