#pragma once

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
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
}

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include "SystemCallbacks.h"

class AudioConverter
{
	std::string file;
	std::string outputFile;
	LogCallback debugLog;

	public:
		AudioConverter(std::string filePath, std::string outFile);
		~AudioConverter();

		void SetDebugLog(LogCallback callback);
		int Convert();
};
