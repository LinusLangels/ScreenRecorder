
#include "AudioConverter.h"

const char *get_error_text(const int error)
{
	static char error_buffer[255];
	av_strerror(error, error_buffer, sizeof(error_buffer));
	return error_buffer;
}

AudioConverter::AudioConverter(std::string filePath, std::string outFile) {
	file = filePath;
	outputFile = outFile;
}

AudioConverter::~AudioConverter() {
}

void AudioConverter::SetDebugLog(LogCallback callback)
{
	debugLog = callback;
}

int AudioConverter::Convert() {
	av_register_all();

	int error;
	AVFormatContext	*pFormatCtx = NULL;

	//Open the input file to read from it.
	if ((error = avformat_open_input(&pFormatCtx, file.c_str(), NULL, NULL)) < 0) {

		if (debugLog != NULL)
		{
			char buffer[256];
			snprintf(buffer, 256, "Could not open input file '%s' (error '%s')", file.c_str(), get_error_text(error));
			debugLog(buffer);
		}

		pFormatCtx = NULL;
		return -1;
	}

	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		if (debugLog != NULL) debugLog("Couldn't find inputStream information.");
		return -1;
	}

	int audioStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) 
		{
			audioStream = i;
			break;
		}
	}

	if (audioStream == -1) {
		if (debugLog != NULL) debugLog("Didn't find a audio stream.");
		return -1;
	}

	AVCodecContext* pCodecCtx = pFormatCtx->streams[audioStream]->codec;
	AVCodec* pCodec = NULL;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		if (debugLog != NULL) debugLog("Codec not found.");
		return -1;
	}

	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		if (debugLog != NULL) debugLog("Could not open codec.");
		return -1;
	}

	int out_sample_rate = pCodecCtx->sample_rate;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	uint64_t out_channel_layout = AV_CH_LAYOUT_MONO;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	int out_nb_samples = pCodecCtx->frame_size;
	int64_t in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);

	char buffer[100];
	snprintf(buffer, 100, "Input samplerate; %d", pCodecCtx->sample_rate);
	debugLog(buffer);

	char buffer2[100];
	snprintf(buffer2, 100, "Input channelcount; %d", pCodecCtx->channels);
	debugLog(buffer2);

	char buffer7[100];
	snprintf(buffer7, 100, "Input sampleformat; %s", av_get_sample_fmt_name(pCodecCtx->sample_fmt));
	debugLog(buffer7);

	char buffer8[100];
	snprintf(buffer8, 100, "Output channelcount; %d", out_channels);
	debugLog(buffer8);

	char buffer10[100];
	snprintf(buffer10, 100, "Output samplesize: %d", out_nb_samples);
	debugLog(buffer10);

	struct SwrContext *au_convert_ctx;
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);

	int contextError;
	if ((contextError = swr_init(au_convert_ctx)) < 0)
	{
		debugLog("Unable to init conversion context");
	}

	AVPacket* packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	AVFrame* pFrame = av_frame_alloc();

	int got_audio_frame;
	int ret;
	int max_dst_samples = 0;
	int dst_linesize = 0;
	bool setupBuffer = true;
	uint8_t **dst_data = NULL;

	AVFormatContext *outputFormatCtx = NULL;
	ret = avformat_alloc_output_context2(&outputFormatCtx, NULL, NULL, outputFile.c_str());
	if (!outputFormatCtx) {
		if (debugLog != NULL)
		{
			char buffer[100];
			snprintf(buffer, 100, "Unable to allocate output context: %s", file.c_str());
			debugLog(buffer);
		}
	}

	AVOutputFormat *outputFormat = NULL;
	outputFormat = outputFormatCtx->oformat;
	AVCodec *audioOutCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);

	if (!audioOutCodec)
	{
		debugLog("Unable to find output codec");
	}

	AVStream* stream = NULL;

	//Create a new audio stream in the output file container.
	if (!(stream = avformat_new_stream(outputFormatCtx, audioOutCodec))) {
		if (debugLog != NULL) debugLog("Could not create new stream.");

		error = AVERROR(ENOMEM);
	}

	int outStreamIndex = stream->index;

	AVCodecContext *outputContext = stream->codec;
	outputContext->sample_fmt = AV_SAMPLE_FMT_S16;
	outputContext->channel_layout = out_channel_layout;
	outputContext->channels = av_get_channel_layout_nb_channels(out_channel_layout);
	outputContext->sample_rate = out_sample_rate;
	outputContext->codec_type = AVMEDIA_TYPE_AUDIO;
	outputContext->frame_size = 4096;

	if (outputFormat->flags & AVFMT_GLOBALHEADER)
		outputContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(outputContext, audioOutCodec, NULL)<0) {
		if (debugLog != NULL) debugLog("Could not open wav codec.");
		return -1;
	}

	//Open output file for writing.
	if (avio_open(&outputFormatCtx->pb, outputFile.c_str(), AVIO_FLAG_READ_WRITE) < 0)
	{
		if (debugLog != NULL) debugLog("Failed to open output file!");
	}

	if (avformat_write_header(outputFormatCtx, NULL) < 0) {
		if (debugLog != NULL) debugLog("Error occurred when writing header data to output file");
		return -1;
	}

	AVFrame* outFrame = NULL;
	uint8_t* outSamples = NULL;
	AVPacket outPacket;

	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == audioStream) {

			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_audio_frame, packet);

			if (ret < 0) {
				if (debugLog != NULL) debugLog("Error in decoding audio frame");
				break;
			}
			if (got_audio_frame > 0) {

				//Only needs to happen once. Need first frame to get initial size.
				if (setupBuffer)
				{
					ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, out_channels, pFrame->nb_samples, out_sample_fmt, 0);
					setupBuffer = false;
				}

				int dst_nb_samples = av_rescale_rnd(swr_get_delay(au_convert_ctx, pCodecCtx->sample_rate) +
					pFrame->nb_samples, out_sample_rate, pCodecCtx->sample_rate, AV_ROUND_UP);

				if (dst_nb_samples > max_dst_samples)
				{
					av_free(dst_data[0]);
					ret = av_samples_alloc(dst_data, &dst_linesize, out_channels, dst_nb_samples, out_sample_fmt, 0);
					max_dst_samples = dst_nb_samples;

					if (outFrame)
					{
						debugLog("Free previous out frame");
						av_frame_free(&outFrame);
					}
					outFrame = av_frame_alloc();
					outFrame->nb_samples = dst_nb_samples;
					outFrame->format = out_sample_fmt;
					outFrame->channel_layout = out_channel_layout;

					if (outSamples)
					{
						debugLog("Free previous out sample buffer");
						av_freep(&outSamples);
					}
					int outBufferSize = av_samples_get_buffer_size(NULL, out_channels, dst_nb_samples, out_sample_fmt, 0);
					outSamples = (uint8_t*)av_malloc(outBufferSize);
					avcodec_fill_audio_frame(outFrame, out_channels, out_sample_fmt, outSamples, outBufferSize, 0);

					av_new_packet(&outPacket, outBufferSize);
				}

				swr_convert(au_convert_ctx, dst_data, dst_nb_samples, (const uint8_t **)pFrame->data, pFrame->nb_samples);

				int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, out_channels, dst_nb_samples, out_sample_fmt, 0);
				memcpy(outSamples, dst_data[0], dst_bufsize);

				int encoded_frame = 0;
				ret = avcodec_encode_audio2(outputContext, &outPacket, outFrame, &encoded_frame);

				if (ret < 0)
				{
					debugLog("Error encoding out data!");
					break;
				}

				if (encoded_frame)
				{
					outPacket.stream_index = outStreamIndex;
					ret = av_write_frame(outputFormatCtx, &outPacket);
					av_packet_unref(&outPacket);
				}

				//int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, out_channels, dst_nb_samples, out_sample_fmt, 0);
				//fwrite(outSamples, 1, dst_bufsize, pFile);
			}
		}
		av_free_packet(packet);
	}

	av_write_trailer(outputFormatCtx);

	avcodec_close(pCodecCtx);

	avcodec_close(outputContext);

	av_free(dst_data[0]);
	av_freep(&outSamples);

	av_frame_free(&pFrame);
	av_frame_free(&outFrame);

	swr_free(&au_convert_ctx);

	if (pFormatCtx) {
		if (debugLog != NULL) debugLog("Close audio file input");
		avformat_close_input(&pFormatCtx);
	}

	if (outputFormatCtx) {
		if (debugLog != NULL) debugLog("Close output file");
		avio_close(outputFormatCtx->pb);

		if (debugLog != NULL) debugLog("Free output context");
		av_free(outputContext);
	}

	return 0;
}
