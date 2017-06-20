//
// Created by Linus on 2017-03-10.
//

#ifndef ANDROIDNATIVECAPTURING_SIMPLEMUXER_H
#define ANDROIDNATIVECAPTURING_SIMPLEMUXER_H

#include "SystemCallbacks.h"
#include <string>

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
	#include "libswresample/swresample.h"
}

typedef struct StreamSource {
    AVFormatContext *formatContext;
    AVStream *inputStream;
} MuxSource;

typedef struct StreamOutput {
    AVFormatContext *formatContext;
    AVStream *videoStream;
    AVStream *audioStream;
    uint8_t *picture_buffer;
    AVFrame *outputFrame;
} MuxDestination;

class Muxer{
    std::string videoFile;
    std::string audioFile;
    std::string debugPath;
    LogCallback debugLog;

    MuxSource* OpenInputSource(const char* file);
    MuxDestination* OpenOutputFile(const char* outFile, MuxSource *videoSource);

public:
    Muxer(std::string inputVideoFile, std::string inputAudioFile);
    ~Muxer();

    int startMuxing(std::string outputFile);
    void SetDebugPath(std::string path);
    void SetDebugLog(LogCallback callback);
};
#endif //ANDROIDNATIVECAPTURING_SIMPLEMUXER_H
