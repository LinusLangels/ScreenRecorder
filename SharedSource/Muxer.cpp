//
// Created by Linus on 2017-03-10.
//


#include "Muxer.h"
#include "AACEncoder.h"

Muxer::Muxer(std::string inputVideoFile, std::string inputAudioFile) {
    this->videoFile = inputVideoFile;
    this->audioFile = inputAudioFile;
    this->debugLog = NULL;
}

Muxer::~Muxer() {
    if (debugLog != NULL) debugLog("Destroyed muxer!");
}

int Muxer::startMuxing(std::string outputFile) {

    if (debugLog != NULL) debugLog("Starting muxer!");

    av_register_all();
    avcodec_register_all();

    //Open input source files.
    MuxSource *videoSource = OpenInputSource(videoFile.c_str());

    //Create output context.
    MuxDestination *output = OpenOutputFile(outputFile.c_str(), videoSource);

    AACEncoder *audioEncoder = new AACEncoder(audioFile, output->formatContext, debugLog);

    //Open output file for writing.
    if (avio_open(&output->formatContext->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0)
    {
        if (debugLog != NULL) debugLog("Failed to open output file!");
        return -1;
    }

    int result = avformat_write_header(output->formatContext, NULL);
    if (result < 0) {
        if (debugLog != NULL)
        {
            char errorBuffer[256];
            char buffer[256];
            snprintf(buffer, 256, "Error occurred when writing header data to output file, %s", av_make_error_string(errorBuffer, 256, result));
            debugLog(buffer);
        }
        return -1;
    }

    AVPacket videoPacket;
    av_init_packet(&videoPacket);
    videoPacket.data = NULL;
    videoPacket.size = 0;
    int64_t cur_pts = 0;

    while(av_read_frame(videoSource->formatContext, &videoPacket) >= 0){
        AVStream* outputStream = output->videoStream;
        AVStream* inputStream = videoSource->inputStream;
        int outStreamIndex = outputStream->index;

        videoPacket.pts = av_rescale_q_rnd(videoPacket.pts, inputStream->time_base, outputStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        videoPacket.dts = av_rescale_q_rnd(videoPacket.dts, inputStream->time_base, outputStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        videoPacket.duration = av_rescale_q(videoPacket.duration, inputStream->time_base, outputStream->time_base);
        videoPacket.pos = -1;
        videoPacket.stream_index = outStreamIndex;

        //Tracking current video timestamp.
        cur_pts = videoPacket.pts;

        if (av_interleaved_write_frame(output->formatContext, &videoPacket) < 0) {
            if (debugLog != NULL) debugLog("Error muxing packet");
            break;
        }

        while (audioEncoder->ShouldEncodeAudio(outputStream, cur_pts) == 1)
        {
            int result = audioEncoder->EncodeAudio();

            if (result < 0){
                if (debugLog != NULL) debugLog("No more audio left to encode");
                break;
            }
        }
    }

    //This will delete all allocated memory associated with audio encoder.
    delete audioEncoder;

    if ((av_write_trailer(output->formatContext)) < 0) {
        if (debugLog != NULL) debugLog("Could not write output file trailer");
    }

    if (videoSource->formatContext){
        if (debugLog != NULL) debugLog("Close video file input");
        avformat_close_input(&(videoSource->formatContext));
    }

    if (output->formatContext){
        if (debugLog != NULL) debugLog("Closing output context");
        avio_close(output->formatContext->pb);
        avformat_free_context(output->formatContext);
    }

    free(videoSource);
    free(output);
    
    return 0;
}

MuxSource* Muxer::OpenInputSource(const char* file) {
    //Begin open input file.
    AVFormatContext	*pFormatCtx = NULL;
    if(avformat_open_input(&pFormatCtx, file, NULL, NULL) != 0)
    {
        if (debugLog != NULL) debugLog("Couldn't open input inputStream.");
        return NULL;
    }

    if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
        if (debugLog != NULL) debugLog("Couldn't find inputStream information.");
        return NULL;
    }

    MuxSource *params = (MuxSource*)malloc((sizeof(MuxSource)));
    params->formatContext = pFormatCtx;

    int i = 0;
    for(i=0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            AVStream *in_stream = pFormatCtx->streams[i];
            params->inputStream = in_stream;

            break;
        }
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
        {
            AVStream *in_stream = pFormatCtx->streams[i];
            params->inputStream = in_stream;

            break;
        }
    }

    return params;
}

MuxDestination* Muxer::OpenOutputFile(const char* outFile, MuxSource *videoSource) {
    AVFormatContext *outputFormatCtx = NULL;
    avformat_alloc_output_context2(&outputFormatCtx, NULL, NULL, outFile);
    if (!outputFormatCtx) {
        if (debugLog != NULL) debugLog("Could not deduce output format from file extension");
    }

    AVOutputFormat *outputFormat = NULL;
    outputFormat = outputFormatCtx->oformat;

    AVStream *videoStream = avformat_new_stream(outputFormatCtx, videoSource->inputStream->codec->codec);
    if (!videoStream) {
        if (debugLog != NULL) debugLog("Could not allocate video output stream");
    }

    if (avcodec_copy_context(videoStream->codec, videoSource->inputStream->codec) < 0) {
        if (debugLog != NULL) debugLog("Failed to copy context from input to output stream codec context");
    }

    videoStream->codec->codec_tag = 0;
    //Some formats want inputStream headers to be separate.
    if (outputFormatCtx->oformat->flags & AVFMT_GLOBALHEADER){
        if (debugLog != NULL) debugLog("Add separate video stream headers!");
        videoStream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(outputFormatCtx, 0, outFile, 1);

    MuxDestination *params = (MuxDestination*)malloc((sizeof(MuxDestination)));
    params->formatContext = outputFormatCtx;
    params->videoStream = videoStream;

    if (debugLog != NULL) debugLog("Success in creating output context");

    return params;
}

void Muxer::SetDebugPath(std::string path) {
    debugPath = path;
}

void Muxer::SetDebugLog(LogCallback callback){
    debugLog = callback;
}
