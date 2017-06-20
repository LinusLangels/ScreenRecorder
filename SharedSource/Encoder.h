//
//  Created by Linus Langels on 19/04/17.
//

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <math.h>
#include <queue>
#include <inttypes.h>
#include "FramePool.h"
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
	#include <libavutil/opt.h>
	#include "libavutil/frame.h"
}

enum CapturingCodec { H264 = 0, MPEG4 = 1, GIF = 2 };

typedef struct EncodeStream {
    AVFormatContext *formatContext;
    AVStream *stream;
    AVCodec *codec;
    uint8_t *picture_buffer;
    AVFrame *outputFrame;
} EncodeStream;

class Encoder{
    std::string debugPath;
    LogCallback debugLog;
    
	std::string videoFile;
    int width;
    int height;
    int framerate;
    int bitrate;
    int iframeinterval;
    int frameCount;
    bool flipV;
	CapturingCodec captureCodec;
    
    EncodeStream* encodeStream;
    AVFrame *encode_frame;
    uint8_t *frame_data;
    int64_t codecTime;
    FramePool *framePool;
    std::queue <FrameObject_t*> processingFrames;
    uint8_t *flippedPixelBuffer;
    
    //State needed for gif generation.
    struct SwsContext *frameScaleConverter;
    AVFrame *rescaleFrame;
	bool isRescaled;
    
    EncodeStream* OpenOutputFile(const char* file);
	int ConfigureOutputVideo(EncodeStream *output, int contextWidth, int contextHeight);
    int OpenOutputVideoCodec(EncodeStream *output);
    int flush_encoder(AVFormatContext *fmt_ctx, int stream_index, int64_t pts);
    int EncodeFrame(AVFrame *frame);
    
public:
    Encoder(std::string videoFile, CapturingCodec codec, int encodeBitrate, int iframeinterval, bool flipVertical);
    ~Encoder();
    
    int EncodeFrames(int maxFrames);
	int StartEncoding(int inputWidth, int inputHeight, int outputWidth, int outputHeight, int inputFramerate);
    int StopEncoding();
    int InsertFrame(uint8_t *frame, int bytesPerPixel, int64_t timeStamp);
    void SetDebugPath(std::string path);
    void SetDebugLog(LogCallback callback);
};
