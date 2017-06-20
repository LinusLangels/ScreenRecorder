#include "AACEncoder.h"

AACEncoder::AACEncoder(std::string path, AVFormatContext *context, LogCallback callback) {
    inputFile = path;
    targetCcontext = context;
    debugLog = callback;

    //Open the input file for reading.
    if (open_input(inputFile.c_str(), &inputContext, &inputCodecContext))
    {
        if (debugLog != NULL) debugLog("Failed to open input file");
    }

    //Open the output file for writing.
    if (configure_output(targetCcontext, inputCodecContext, &outputCodecContext, &audioStream) < 0)
    {
        if (debugLog != NULL) debugLog("failed setup");
    }

    //Initialize the resampler to be able to convert audio sample formats.
    if (init_resampler(inputCodecContext, outputCodecContext, &resample_context))
    {
        if (debugLog != NULL) debugLog("Failed to init resampler");
    }

    //Initialize the FIFO buffer to store audio samples to be encoded.
    if (init_fifo(&fifo, outputCodecContext))
    {
        if (debugLog != NULL) debugLog("Failed to init fifo");
    }
}

AACEncoder::~AACEncoder(){

    if (debugLog != NULL) debugLog("Destroying aac encoder!");

    if (fifo)
        av_audio_fifo_free(fifo);

    swr_free(&resample_context);

    if (outputCodecContext)
    avcodec_close(outputCodecContext);

    if (inputCodecContext)
    avcodec_close(inputCodecContext);

    if (inputContext)
    avformat_close_input(&inputContext);
}

int AACEncoder::open_input(const char *filename,
                           AVFormatContext **input_format_context,
                           AVCodecContext **input_codec_context) {
    AVCodec *input_codec;
    int error;

    //Open the input file to read from it.
    if ((error = avformat_open_input(input_format_context, filename, NULL, NULL)) < 0) {
        
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not open input file '%s' (error '%s')", filename, get_error_text(error));
            debugLog(buffer);
        }
        
        *input_format_context = NULL;
        return error;
    }

    //Get information on the input file (number of streams etc.).
    if ((error = avformat_find_stream_info(*input_format_context, NULL)) < 0) {
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not open find stream info (error '%s')", get_error_text(error));
            debugLog(buffer);
        }
        
        avformat_close_input(input_format_context);
        return error;
    }

    //Make sure that there is only one stream in the input file.
    if ((*input_format_context)->nb_streams != 1) {
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Expected one audio input stream, but found %d", (*input_format_context)->nb_streams);
            debugLog(buffer);
        }
        
        avformat_close_input(input_format_context);
        return AVERROR_EXIT;
    }

    //Find a decoder for the audio stream.
    if (!(input_codec = avcodec_find_decoder((*input_format_context)->streams[0]->codec->codec_id))) {
        if (debugLog != NULL) debugLog("Could not find input codec");
        
        avformat_close_input(input_format_context);
        return AVERROR_EXIT;
    }

    //Open the decoder for the audio stream to use it later.
    if ((error = avcodec_open2((*input_format_context)->streams[0]->codec, input_codec, NULL)) < 0) {
        
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not open input codec (error '%s')", get_error_text(error));
            debugLog(buffer);
        }
        
        avformat_close_input(input_format_context);
        return error;
    }

    //Save the decoder context for easier access later.
    *input_codec_context = (*input_format_context)->streams[0]->codec;

    return 0;
}

int AACEncoder::configure_output(AVFormatContext *output_context,
                                 AVCodecContext *input_codec_context,
                                 AVCodecContext **output_codec_context,
                                 AVStream **audio_stream)
{

    AVStream *stream               = NULL;
    AVCodec *output_codec          = NULL;
    int error;

    //Find the encoder to be used by its name.
    if (!(output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
        if (debugLog != NULL) debugLog("Could not find an AAC encoder.");
    }

    //Create a new audio stream in the output file container.
    if (!(stream = avformat_new_stream(output_context, output_codec))) {
        if (debugLog != NULL) debugLog("Could not create new stream.");
        
        error = AVERROR(ENOMEM);
    }

    //Save the encoder context for easier access later.
    *output_codec_context = stream->codec;
    (*output_codec_context)->channels       = input_codec_context->channels;
    (*output_codec_context)->channel_layout = av_get_default_channel_layout(input_codec_context->channels);
    (*output_codec_context)->sample_rate    = input_codec_context->sample_rate;
    (*output_codec_context)->sample_fmt     = output_codec->sample_fmts[0];
    (*output_codec_context)->bit_rate       = 96000;

    //Allow the use of the experimental AAC encoder
    (*output_codec_context)->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    //Set the sample rate for the container.
    stream->time_base.den = input_codec_context->sample_rate;
    stream->time_base.num = 1;

    //Assign output stream
    *audio_stream = stream;

    if (output_context->oformat->flags & AVFMT_GLOBALHEADER){
        if (debugLog != NULL) debugLog("Add sepeate audio stream headers");
        
        (*output_codec_context)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((error = avcodec_open2(*output_codec_context, output_codec, NULL)) < 0) {
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not open output codec (error '%s')", get_error_text(error));
            debugLog(buffer);
        }
        
        return -1;
    }

    return 0;
}

void AACEncoder::init_packet(AVPacket *packet) {
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;
}

int AACEncoder::init_input_frame(AVFrame **frame) {
    if (!(*frame = av_frame_alloc())) {
        if (debugLog != NULL) debugLog("Could not allocate input frame.");
        return AVERROR(ENOMEM);
    }
    return 0;
}

int AACEncoder::init_resampler(AVCodecContext *input_codec_context, AVCodecContext *output_codec_context, SwrContext **resample_context) {
    int error;

    *resample_context = swr_alloc_set_opts(NULL,
                                           av_get_default_channel_layout(output_codec_context->channels),
                                           output_codec_context->sample_fmt,
                                           output_codec_context->sample_rate,
                                           av_get_default_channel_layout(input_codec_context->channels),
                                           input_codec_context->sample_fmt,
                                           input_codec_context->sample_rate,
                                           0, NULL);
    if (!*resample_context) {
        if (debugLog != NULL) debugLog("Could not allocate resample context.");
        return AVERROR(ENOMEM);
    }

    if ((error = swr_init(*resample_context)) < 0) {
        if (debugLog != NULL) debugLog("Could not allocate resample context.");
        swr_free(resample_context);
        return error;
    }
    return 0;
}

int AACEncoder::init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context)
{
    //Create the FIFO buffer based on the specified output sample format.
    if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
                                      output_codec_context->channels, 1))) {
        if (debugLog != NULL) debugLog("Could not allocate FIFO");
        return AVERROR(ENOMEM);
    }
    return 0;
}

int AACEncoder::decode_audio_frame(AVFrame *frame,
                              AVFormatContext *input_format_context,
                              AVCodecContext *input_codec_context,
                              int *data_present, int *finished)
{
    //Packet used for temporary storage.
    AVPacket input_packet;
    int error;
    init_packet(&input_packet);

    //Read one audio frame from the input file into a temporary packet.
    if ((error = av_read_frame(input_format_context, &input_packet)) < 0) {
        //If we are at the end of the file, flush the decoder below.
        if (error == AVERROR_EOF)
            *finished = 1;
        else {
            if (debugLog != NULL)
            {
                char buffer [256];
                snprintf(buffer, 256, "Could not read frame (error '%s')\n", get_error_text(error));
                debugLog(buffer);
            }
            
            return error;
        }
    }

    /**
     * Decode the audio frame stored in the temporary packet.
     * The input audio stream decoder is used to do this.
     * If we are at the end of the file, pass an empty packet to the decoder
     * to flush it.
     */
    if ((error = avcodec_decode_audio4(input_codec_context, frame, data_present, &input_packet)) < 0) {
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not decode frame (error '%s')\n", get_error_text(error));
            debugLog(buffer);
        }
        
        av_packet_unref(&input_packet);
        return error;
    }

	//Make sure all samples are flushed from decoder.
    if (*finished && *data_present)
        *finished = 0;

    av_packet_unref(&input_packet);
    return 0;
}

int AACEncoder::init_converted_samples(uint8_t ***converted_input_samples,
                                  AVCodecContext *output_codec_context,
                                  int frame_size)
{
    int error;

    if (!(*converted_input_samples = (uint8_t**)calloc(output_codec_context->channels, sizeof(**converted_input_samples)))) {
        if (debugLog != NULL) debugLog("Could not allocate converted input sample pointers");
        return AVERROR(ENOMEM);
    }

    if ((error = av_samples_alloc(*converted_input_samples, NULL,
                                  output_codec_context->channels,
                                  frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
        
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not allocate converted input samples (error '%s')\n", get_error_text(error));
            debugLog(buffer);
        }
        
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

int AACEncoder::convert_samples(const uint8_t **input_data,
                           uint8_t **converted_data, const int frame_size,
                           SwrContext *resample_context)
{
    int error;

    //Convert the samples using the resampler.
    if ((error = swr_convert(resample_context,
                             converted_data, frame_size,
                             input_data    , frame_size)) < 0) {
        
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not convert input samples (error '%s')\n", get_error_text(error));
            debugLog(buffer);
        }
        
        return error;
    }

    return 0;
}

int AACEncoder::add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **converted_input_samples,
                               const int frame_size)
{
    int error;

	//Make sure our FIFO can hold all the samples
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
        if (debugLog != NULL) debugLog("Could not reallocate FIFO");
        return error;
    }

    //Store the new samples in the FIFO buffer.
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                            frame_size) < frame_size) {
        if (debugLog != NULL) debugLog("Could not write data to FIFO");
        return AVERROR_EXIT;
    }
    return 0;
}

int AACEncoder::read_decode_convert_and_store(AVAudioFifo *fifo,
                                         AVFormatContext *input_format_context,
                                         AVCodecContext *input_codec_context,
                                         AVCodecContext *output_codec_context,
                                         SwrContext *resampler_context,
                                         int *finished)
{
    //Temporary storage of the input samples of the frame read from the file.
    AVFrame *input_frame = NULL;
    //Temporary storage for the converted input samples.
    uint8_t **converted_input_samples = NULL;
    int data_present;
    int ret = AVERROR_EXIT;

    if (init_input_frame(&input_frame))
        goto cleanup;
    
    if (decode_audio_frame(input_frame, input_format_context,
                           input_codec_context, &data_present, finished))
        goto cleanup;
    
	//Check for error or end.
    if (*finished && !data_present) {
        ret = 0;
        goto cleanup;
    }

    //If there is decoded data, convert and store it
    if (data_present) {
        //Initialize the temporary storage for the converted input samples. */
        if (init_converted_samples(&converted_input_samples, output_codec_context,
                                   input_frame->nb_samples))
            goto cleanup;

        //Convert the input samples to the desired output sample format.
        if (convert_samples((const uint8_t**)input_frame->extended_data, converted_input_samples,
                            input_frame->nb_samples, resampler_context))
            goto cleanup;

        // Add the converted input samples to the FIFO buffer for later processing.
        if (add_samples_to_fifo(fifo, converted_input_samples,
                                input_frame->nb_samples))
            goto cleanup;
        ret = 0;
    }
    ret = 0;

    cleanup:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }
    av_frame_free(&input_frame);

    return ret;
}

int AACEncoder::init_output_frame(AVFrame **frame,
                             AVCodecContext *output_codec_context,
                             int frame_size)
{
    int error;

    if (!(*frame = av_frame_alloc())) {
        if (debugLog != NULL) debugLog("Could not allocate output frame");
        return AVERROR_EXIT;
    }

    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format         = output_codec_context->sample_fmt;
    (*frame)->sample_rate    = output_codec_context->sample_rate;

    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could allocate output frame samples (error '%s')\n", get_error_text(error));
            debugLog(buffer);
        }
        
        av_frame_free(frame);
        return error;
    }

    return 0;
}

int AACEncoder::encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present)
{
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);

    if (frame) {
        frame->pts = audioPTS;
        audioPTS += frame->nb_samples;
    }

    if ((error = avcodec_encode_audio2(output_codec_context, &output_packet, frame, data_present)) < 0) {
        
        if (debugLog != NULL)
        {
            char buffer [256];
            snprintf(buffer, 256, "Could not encode frame (error '%s')\n", get_error_text(error));
            debugLog(buffer);
        }
        
        av_packet_unref(&output_packet);
        return error;
    }

    //Write one audio frame from the temporary packet to the output file.
    if (*data_present)
    {
        //Make sure correct stream index is set when we interleave frames.
        output_packet.stream_index = audioStream->index;

        if ((error = av_interleaved_write_frame(output_format_context, &output_packet)) < 0) {
            
            if (debugLog != NULL)
            {
                char buffer [256];
                snprintf(buffer, 256, "Could not write frame (error '%s')\n", get_error_text(error));
                debugLog(buffer);
            }
            
            av_packet_unref(&output_packet);
            return error;
        }

        av_packet_unref(&output_packet);
    }

    return 0;
}

int AACEncoder::load_encode_and_write(AVAudioFifo *fifo,
                                 AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context)
{
    //Temporary storage of the output samples of the frame written to the file.
    AVFrame *output_frame;
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 output_codec_context->frame_size);
    int data_written;

    //Initialize temporary storage for one output frame.
    if (init_output_frame(&output_frame, output_codec_context, frame_size))
        return AVERROR_EXIT;

    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
        if (debugLog != NULL) debugLog("Could not read data from FIFO");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    //Encode one frame worth of audio samples.
    if (encode_audio_frame(output_frame, output_format_context, output_codec_context, &data_written)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    av_frame_free(&output_frame);
    return 0;
}

int AACEncoder::EncodeAudio() 
{
    const int output_frame_size = outputCodecContext->frame_size;
    int finished = 0;

    //Fill the fifo.
    while (av_audio_fifo_size(fifo) < output_frame_size) {

        if (read_decode_convert_and_store(fifo, inputContext, inputCodecContext, outputCodecContext, resample_context, &finished))
        {
            break;
        }

        if (finished){
            if (debugLog != NULL) debugLog("End of audio stream!");
            break;
        }

    }

    //Empty the fifo into output codec.
    while (av_audio_fifo_size(fifo) >= output_frame_size || (finished && av_audio_fifo_size(fifo) > 0)){
        if (load_encode_and_write(fifo, targetCcontext, outputCodecContext))
        {
            break;
        }
    }

    if (finished)
    {
        int data_written;

        //Flush the encoder as it may have delayed frames.
        do
        {
            if (encode_audio_frame(NULL, targetCcontext, outputCodecContext, &data_written))
            {
                break;
            }
        } while (data_written);

        if (debugLog != NULL) debugLog("End of input audio!");

        return -1;
    }

    return 0;
}

int AACEncoder::ShouldEncodeAudio(AVStream* videoStream, int64_t videoPTS)
{
    if (audioStream) {
        double video_time = (double)(videoPTS * videoStream->time_base.num) / videoStream->time_base.den;
        double audio_time = (double)(audioPTS * audioStream->time_base.num) / audioStream->time_base.den;

        if (audio_time < video_time) {
            return 1;
        }

        return 0;
    }
    else {
        return -1;
    }
}

int AACEncoder::GetAudioPTS(){
    return audioPTS;
}
