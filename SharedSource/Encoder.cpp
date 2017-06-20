//
// Created by Linus on 2017-02-23.
//

#include "Encoder.h"
#include <inttypes.h>
#include "DebugTools.h"
#include "RGB2YUV420.h"

Encoder::Encoder(std::string videoFile, CapturingCodec codec, int encodeBitrate, int iframeinterval, bool flipVertical)
{
    this->videoFile = videoFile;
	this->captureCodec = codec;
    this->bitrate = encodeBitrate;
    this->iframeinterval = iframeinterval;
    this->flipV = flipVertical;
	this->captureCodec = codec;
    
    debugLog = NULL;
    encodeStream = nullptr;
    encode_frame = nullptr;
    frame_data = nullptr;
    framePool = nullptr;
    flippedPixelBuffer = nullptr;
    frameScaleConverter = nullptr;
    rescaleFrame = nullptr;
}

Encoder::~Encoder() {
    if (debugLog != NULL) debugLog("Destroyed encoder!");
}

int Encoder::StartEncoding(int inputWidth, int inputHeight, int outputWidth, int outputHeight, int inputFramerate) 
{
	this->width = inputWidth;
	this->height = inputHeight;
	this->framerate = inputFramerate;

    if (debugLog != NULL) debugLog("Start encoding");
    
    //Initialize all muxers/demuxers
    av_register_all();
    
    //Register all available codecs.
    avcodec_register_all();
    
    //Detect device hardware to allow for set_opt
    av_get_cpu_flags();
    
    this->encodeStream = OpenOutputFile(videoFile.c_str());
    
    if (encodeStream == nullptr) {
        if (debugLog != NULL) debugLog("Unable to setup output encoder");
        
        return -1;
    }
    
    //Set codec options.
    if (ConfigureOutputVideo(this->encodeStream, outputWidth, outputHeight) < 0) {
        if (debugLog != NULL) debugLog("Unable to configure video output");
    }
    
    if (OpenOutputVideoCodec(this->encodeStream) < 0) {
        if (debugLog != NULL) debugLog("Unable to open output video codec");
    }
    
    //Open output file for writing.
    if (avio_open(&this->encodeStream->formatContext->pb, videoFile.c_str(), AVIO_FLAG_READ_WRITE) < 0)
    {
        if (debugLog != NULL) debugLog("Failed to open output file!");
    }
    
    if (avformat_write_header(encodeStream->formatContext, NULL) < 0) {
        if (debugLog != NULL) debugLog("Error occurred when writing header data to output file");
        return 1;
    }
    
    //Pool to hold our incoming raw rgba frames.
    int bufferSize = width * height * 4;
    framePool = new FramePool(10, bufferSize);
    
    //This is the main frame we convert and fill from the capturer output.
    int numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, width, height);
    frame_data = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    encode_frame = av_frame_alloc();
    avpicture_fill((AVPicture *)encode_frame, frame_data, AV_PIX_FMT_YUV420P, width, height);
    encode_frame->format = AV_PIX_FMT_YUV420P;
    encode_frame->width = width;
    encode_frame->height = height;
    
    if (debugLog != NULL)
    {
        char buffer [100];
        snprintf(buffer, 100, "Data size: %d", numBytes);
        debugLog(buffer);
    }
    
	if (inputWidth != outputWidth || inputHeight != outputHeight)
	{
		AVCodecContext *outputContext = this->encodeStream->stream->codec;
		rescaleFrame = av_frame_alloc();
		rescaleFrame->format = outputContext->pix_fmt;
		rescaleFrame->width = outputWidth;
		rescaleFrame->height = outputHeight;
		av_image_alloc(rescaleFrame->data, rescaleFrame->linesize, outputWidth, outputHeight, outputContext->pix_fmt, 1);
		frameScaleConverter = sws_getContext(width, height, AV_PIX_FMT_YUV420P, outputWidth, outputHeight, outputContext->pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
	
		isRescaled = true;
	}
    
    //Init tracking variables.
    frameCount = 0;
    codecTime = 0;
    
    if (flipV){
        flippedPixelBuffer = (uint8_t*)malloc(width * height * 4);
    }
    else
    {
        flippedPixelBuffer = nullptr;
    }
    
    if (debugLog != NULL) debugLog("Encoder is ready!");
    
    return 0;
}

int Encoder::StopEncoding() 
{    
    if (debugLog != NULL) debugLog("Stop encoding");
    
    //Encode the remaining cached frames.
    if (debugLog != NULL)
    {
        char buffer [100];
        snprintf(buffer, 100, "Encoding cached frames %u", processingFrames.size());
        debugLog(buffer);
    }
    
    EncodeFrames(processingFrames.size());

    //Protect us against division by zero errors.
    if (frameCount == 0)
    {
        frameCount = 1;
    }
    
    int64_t averagePTS = codecTime / frameCount;
    
    if (debugLog != NULL)
    {
        char buffer [100];
        snprintf(buffer, 100, "Average pts: %lld", averagePTS);
        debugLog(buffer);
    }
    
    flush_encoder(encodeStream->formatContext, encodeStream->stream->index, averagePTS);
    
    //Clear all memory for buffered images.
    delete framePool;
    
    av_write_trailer(encodeStream->formatContext);
    
    if (encodeStream->stream){

        if (debugLog != NULL) debugLog("Closing output video codec");

        avcodec_close(encodeStream->stream->codec);
    }
    
    if (encodeStream->formatContext){
        AVFormatContext *outputContext = encodeStream->formatContext;
        
        if (debugLog != NULL) debugLog("Free output streams");
        
        int i;
        for (i = 0; i < outputContext->nb_streams; i++) {
            av_freep(&outputContext->streams[i]->codec);
            av_freep(&outputContext->streams[i]);
        }
        
        if (debugLog != NULL) debugLog("Close output file");
        avio_close(outputContext->pb);
        
        if (debugLog != NULL) debugLog("Free output context");
        av_free(outputContext);
    }
    
    //Free size conversion data.
    sws_freeContext(frameScaleConverter);
    av_freep(&rescaleFrame->data[0]);
    av_frame_free(&rescaleFrame);
    
    if (debugLog != NULL) debugLog("Free yuv buffer data");
    av_free(frame_data);
    
    if (debugLog != NULL) debugLog("free yuv frame");
    av_frame_free(&encode_frame);
    
    if (flippedPixelBuffer)
    {
        free(flippedPixelBuffer);
        flippedPixelBuffer = nullptr;
    }
    
	std::queue <FrameObject_t*>().swap(processingFrames);

    return 0;
}

int Encoder::EncodeFrames(int maxFrames) 
{    
    //Regulates how many frames per step we are allowed to encode.
    int framesEncoded = 0;
    
    while (processingFrames.size() > 0) {
        
        if (framesEncoded >= maxFrames) {
            break;
        }
        
        FrameObject_t *raw_frame = processingFrames.front();
        processingFrames.pop();
        
        uint8_t *rgbaFrame = nullptr;
        
        if (flipV)
        {
            uint8_t *rawframe = raw_frame->frame;
            
            for (int y=0; y < height; y++)
            {
                for (int x=0; x < width; x++)
                {
                    flippedPixelBuffer[(y*width+x)*4+0] = rawframe[((height-1-y)*width+x)*4+0];
                    flippedPixelBuffer[(y*width+x)*4+1] = rawframe[((height-1-y)*width+x)*4+1];
                    flippedPixelBuffer[(y*width+x)*4+2] = rawframe[((height-1-y)*width+x)*4+2];
                    flippedPixelBuffer[(y*width+x)*4+3] = rawframe[((height-1-y)*width+x)*4+3];
                }
            }
            
            rgbaFrame = flippedPixelBuffer;
        }
        else
        {
            rgbaFrame = raw_frame->frame;
        }

        rgb2yuv420(frame_data, rgbaFrame, 4, width, height);
        
        AVFrame *dstFrame = nullptr;
        AVCodecContext *outputCodec = encodeStream->stream->codec;
        
		//Resize input frame to match the codec size.
        if (isRescaled)
        {
            sws_scale(frameScaleConverter,
                      (const uint8_t * const *)encode_frame->data,
                      encode_frame->linesize,
                      0,
                      height,
                      rescaleFrame->data,
                      rescaleFrame->linesize);
            
            dstFrame = rescaleFrame;
        }
        else
        {
            dstFrame = encode_frame;
        }
        
        dstFrame->pts = raw_frame->pts;
        dstFrame->pict_type = AV_PICTURE_TYPE_NONE;
        EncodeFrame(dstFrame);
        
		codecTime += raw_frame->pts;
        framePool->pushFrame(raw_frame);
        
		frameCount++;
        framesEncoded++;
    }
    
    return 0;
}

int Encoder::flush_encoder(AVFormatContext *fmt_ctx, int stream_index, int64_t pts) {
    int ret;
    int got_frame;
    
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
        return 0;
    
    int64_t currentPTS = pts;
    
    while (1) {
        AVPacket enc_pkt;
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2 (fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
        av_frame_free(NULL);
        
        if (ret < 0){
            if (debugLog != NULL) debugLog("Failed to flush frames");
            break;
        }
        
        if (!got_frame){
            ret=0;
            if (debugLog != NULL) debugLog("No flush frames left in codec");
            break;
        }
        
        if (fmt_ctx->streams[stream_index]->codec->coded_frame->key_frame) {
            if (debugLog != NULL) debugLog("Keyframe");
            enc_pkt.flags |= AV_PKT_FLAG_KEY;
        }
        
        if (enc_pkt.pts == AV_NOPTS_VALUE){
            if (debugLog != NULL) debugLog("Setting new flush pts");
            enc_pkt.pts = currentPTS;
        }
        
        enc_pkt.stream_index = stream_index;
        
        if (debugLog != NULL)
        {
            char buffer [100];
            snprintf(buffer, 100, "flush pts: %lld", enc_pkt.pts);
            debugLog(buffer);
            
            snprintf(buffer, 100, "flush duration: %lld", enc_pkt.duration);
            debugLog(buffer);
            
            snprintf(buffer, 100, "Flush Encoder: Succeed to encode 1 frame!\tsize:%5d",enc_pkt.size);
            debugLog(buffer);
        }

        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0){
            if (debugLog != NULL) debugLog("Failed to write flushed frame");
            break;
        }
        
        //Simulated delay between frames.
        currentPTS += pts;
    }
    
    return ret;
}

int Encoder::InsertFrame(uint8_t *frame, int bytesPerPixel, int64_t timeStamp){
    
	AVRational timeScale;
	timeScale.num = 1;
	timeScale.den = 1000;

    int64_t pts = av_rescale_q(timeStamp, timeScale, encodeStream->stream->time_base);
    
    if (framePool->framesAvailable() > 0) {
        FrameObject_t *freeFrame = framePool->popFrame();
        memcpy(freeFrame->frame, frame, width * height * 4);
        freeFrame->pts = pts;
        
        processingFrames.push(freeFrame);
    }
    else
    {
        //Dont allow an infinite size queue.
        if (processingFrames.size() < 20)
        {
            FrameObject_t *newFrame = (FrameObject_t*)malloc((sizeof(FrameObject_t)));
            newFrame->frame = (uint8_t*)(malloc(width * height * 4));
            newFrame->pts = pts;
            
            memcpy(newFrame->frame, frame, width * height * 4);
            
            processingFrames.push(newFrame);
        }
        
        if (debugLog != NULL) debugLog("Out of frames!!!");
    }
    
    return 0;
}

int Encoder::EncodeFrame(AVFrame *frame)
{
    AVCodecContext *outputCodec = encodeStream->stream->codec;
    
    AVPacket pkt;
    int got_output;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    int ret;
    ret = avcodec_encode_video2(outputCodec, &pkt, frame, &got_output);
    if (ret < 0) {

        if (debugLog != NULL)
        {
            char error_buffer[255];
            av_strerror(ret, error_buffer, sizeof(error_buffer));
            
            char buffer [100];
            snprintf(buffer, 100, "Error encoding: %s", error_buffer);
            debugLog(buffer);
        }

        return -1;
    }
    
    if (got_output)
    {
        if (outputCodec->coded_frame->pts != AV_NOPTS_VALUE) {
            pkt.pts = av_rescale_q(outputCodec->coded_frame->pts, outputCodec->time_base, outputCodec->time_base);
        }
        
        if (outputCodec->coded_frame->key_frame) {
            pkt.flags |= AV_PKT_FLAG_KEY;
        }
        
        pkt.stream_index = encodeStream->stream->index;
        
        ret = av_write_frame(encodeStream->formatContext, &pkt);
    }
    else {
        if (debugLog != NULL) debugLog("No output for this frame");
        ret = 0;
    }
    
    if (ret != 0) {
        if (debugLog != NULL) debugLog("Error while writing video frame");
        return -1;
    }
    
    return 0;
}

void Encoder::SetDebugPath(std::string path) {
    debugPath = path;
}

void Encoder::SetDebugLog(LogCallback callback){
    debugLog = callback;
}

EncodeStream* Encoder::OpenOutputFile(const char *file) {
    AVFormatContext *outputFormatCtx = NULL;
    int ret = avformat_alloc_output_context2(&outputFormatCtx, NULL, NULL, file);
    if (!outputFormatCtx) {
        if (debugLog != NULL)
        {
            char buffer [100];
            snprintf(buffer, 100, "Unable to allocate output context: %s", file);
            debugLog(buffer);
        }
    }
    
    AVOutputFormat *outputFormat = NULL;
    outputFormat = outputFormatCtx->oformat;
    
    //Since we cant use assembly level optimization on x86 mpeg4 is actually a faster codec.
    AVCodec *videoCodec = avcodec_find_encoder(outputFormat->video_codec);
    
    if (!videoCodec) {
        if (debugLog != NULL) debugLog("Could not find video codec");
    }
    
    AVStream *videoStream = avformat_new_stream(outputFormatCtx, videoCodec);
    if (!videoStream) {
        if (debugLog != NULL) debugLog("Could not allocate video inputStream");
    }
    videoStream->id = 1;
    
    EncodeStream *params = (EncodeStream*)malloc((sizeof(EncodeStream)));
    params->formatContext = outputFormatCtx;
    params->stream = videoStream;
    params->codec = videoCodec;
    
    if (debugLog != NULL)
    {
        if (debugLog != NULL) debugLog("Success in creating output context");
        
        char buffer [100];
        snprintf(buffer, 100, "Video Codec: %s", params->codec->long_name);
        debugLog(buffer);
    }
    
    return params;
}

int Encoder::ConfigureOutputVideo(EncodeStream *output, int contextWidth, int contextHeight)
{
    AVCodecContext *outputContext = output->stream->codec;
    
    //Initialize the selected codec context with preferred values.
    avcodec_get_context_defaults3(outputContext, output->codec);
    
    outputContext->codec_id = output->codec->id;
    outputContext->bit_rate = bitrate;
    outputContext->width    = contextWidth;     //Resolution must be a multiple of two.
    outputContext->height   = contextHeight;    //Resolution must be a multiple of two.
    
    outputContext->time_base.num = 1000;
    outputContext->time_base.den = (int)(1000.0 * (double)framerate);
    outputContext->gop_size      = 12; /* emit one intra frame every 10 frames at most */
    outputContext->pix_fmt       = AV_PIX_FMT_YUV420P;
    outputContext->max_b_frames = 0;
    
	switch (outputContext->codec_id)
	{
	case AV_CODEC_ID_H264:
		outputContext->qblur = 0.0f;
		outputContext->qmin = 18;
		outputContext->qmax = 28;
		//Faster encoder will result in larger output file. A slower preset will result in smaller filesize but slower encoding.
		av_opt_set(outputContext->priv_data, "preset", "ultrafast", 0);
		break;
	case AV_CODEC_ID_MPEG4:
		outputContext->qmin = 3;
		outputContext->qmax = 10;
		outputContext->qblur = 0.1f;
		break;

	case AV_CODEC_ID_GIF:
		outputContext->gop_size = 1;
		outputContext->bit_rate = 400000;
		outputContext->time_base.num = 1;
		outputContext->time_base.den = framerate;
		outputContext->pix_fmt = AV_PIX_FMT_RGB8;
		break;
	}
    
    if (outputContext->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        outputContext->max_b_frames = 2;
        if (debugLog != NULL) debugLog("Using AV_CODEC_ID_MPEG2VIDEO");
    }
    
    if (outputContext->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        outputContext->mb_decision = 2;
        if (debugLog != NULL) debugLog("Using AV_CODEC_ID_MPEG1VIDEO");
    }
    
    //Some formats want inputStream headers to be separate.
    if (output->formatContext->oformat->flags & AVFMT_GLOBALHEADER){
        if (debugLog != NULL) debugLog("Add separate video stream headers");
        outputContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    
    return 0;
}

int Encoder::OpenOutputVideoCodec(EncodeStream *output){
    AVCodecContext *outputContext = output->stream->codec;
    AVCodec *codec = output->codec;
    
    if (debugLog != NULL)
    {
        char buffer [100];
        snprintf(buffer, 100, "Opening video codec: %s", codec->long_name);
        debugLog(buffer);
    }
    
    if (avcodec_open2(outputContext, codec, NULL) < 0)
    {
        if (debugLog != NULL) debugLog("Could not open output video codec");
        return -1;
    }
    
    if (debugLog != NULL)
    {
        char buffer [100];
        snprintf(buffer, 100, "Codec: %s is open", codec->long_name);
        debugLog(buffer);
    }
    
    return 0;
}
