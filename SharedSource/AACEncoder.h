//
// Created by Linus on 2017-03-14.
//

#ifndef ANDROIDNATIVECAPTURING_TRANSCODE_AAC_H
#define ANDROIDNATIVECAPTURING_TRANSCODE_AAC_H

#include <string>
#include <stdio.h>
#include "SystemCallbacks.h"

extern "C" {
    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"
    #include "libavcodec/avcodec.h"
    #include "libavutil/audio_fifo.h"
    #include "libavutil/avassert.h"
    #include "libavutil/avstring.h"
    #include "libavutil/frame.h"
    #include "libavutil/opt.h"
    #include "libswresample/swresample.h"
}

class AACEncoder {
    std::string inputFile;
    AVFormatContext *targetCcontext;
    int64_t audioPTS = 0;
    LogCallback debugLog;

    AVFormatContext *inputContext = NULL;
    AVCodecContext *inputCodecContext = NULL;
    AVCodecContext *outputCodecContext = NULL;
    AVStream *audioStream = NULL;
    SwrContext *resample_context = NULL;
    AVAudioFifo *fifo = NULL;
    int ret = AVERROR_EXIT;

    int open_input(const char *filename,
                   AVFormatContext **input_format_context,
                   AVCodecContext **input_codec_context);

    int configure_output(AVFormatContext *output_context,
                         AVCodecContext *input_codec_context,
                         AVCodecContext **output_codec_context,
                         AVStream **audio_stream);

    const char *get_error_text(const int error)
    {
        static char error_buffer[255];
        av_strerror(error, error_buffer, sizeof(error_buffer));
        return error_buffer;
    }

    void init_packet(AVPacket *packet);
    int init_input_frame(AVFrame **frame);
    int init_resampler(AVCodecContext *input_codec_context, AVCodecContext *output_codec_context, SwrContext **resample_context);
    int init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context);
    int decode_audio_frame(AVFrame *frame,
                                  AVFormatContext *input_format_context,
                                  AVCodecContext *input_codec_context,
                                  int *data_present, int *finished);
    int init_converted_samples(uint8_t ***converted_input_samples,
                                      AVCodecContext *output_codec_context,
                                      int frame_size);
    int convert_samples(const uint8_t **input_data,
                               uint8_t **converted_data, const int frame_size,
                               SwrContext *resample_context);
    int add_samples_to_fifo(AVAudioFifo *fifo,
                                   uint8_t **converted_input_samples,
                                   const int frame_size);
    int read_decode_convert_and_store(AVAudioFifo *fifo,
                                             AVFormatContext *input_format_context,
                                             AVCodecContext *input_codec_context,
                                             AVCodecContext *output_codec_context,
                                             SwrContext *resampler_context,
                                             int *finished);
    int init_output_frame(AVFrame **frame,
                                 AVCodecContext *output_codec_context,
                                 int frame_size);
    int encode_audio_frame(AVFrame *frame,
                                  AVFormatContext *output_format_context,
                                  AVCodecContext *output_codec_context,
                                  int *data_present);
    int load_encode_and_write(AVAudioFifo *fifo,
                                     AVFormatContext *output_format_context,
                                     AVCodecContext *output_codec_context);


public:
    AACEncoder(std::string path, AVFormatContext *context, LogCallback callback);
    ~AACEncoder();

    int EncodeAudio();
    int ShouldEncodeAudio(AVStream* videoStream, int64_t videoPTS);
    int GetAudioPTS();
};

#endif //ANDROIDNATIVECAPTURING_TRANSCODE_AAC_H
