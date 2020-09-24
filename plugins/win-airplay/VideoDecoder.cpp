#include "VideoDecoder.h"
#include "airplay-server.h"
#include <obs.hpp>

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
	int err = 0;

	if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL,
					  0)) < 0) {
		fprintf(stderr, "Failed to create specified HW device.\n");
		return err;
	}
	ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
					const enum AVPixelFormat *pix_fmts)
{
	const enum AVPixelFormat *p;

	for (p = pix_fmts; *p != -1; p++) {
		if (*p == hw_pix_fmt)
			return *p;
	}

	fprintf(stderr, "Failed to get HW surface format.\n");
	return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet)
{
	AVFrame *frame = NULL, *sw_frame = NULL;
	AVFrame *tmp_frame = NULL;
	uint8_t *buffer = NULL;
	int size;
	int ret = 0;

	ret = avcodec_send_packet(avctx, packet);
	if (ret < 0) {
		fprintf(stderr, "Error during decoding\n");
		return ret;
	}

	while (1) {
		if (!(frame = av_frame_alloc()) ||
		    !(sw_frame = av_frame_alloc())) {
			fprintf(stderr, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		ret = avcodec_receive_frame(avctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			return 0;
		} else if (ret < 0) {
			fprintf(stderr, "Error while decoding\n");
			goto fail;
		}

		if (frame->format == hw_pix_fmt) {
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(sw_frame, frame,
							    0)) < 0) {
				fprintf(stderr,
					"Error transferring the data to system memory\n");
				goto fail;
			}
			tmp_frame = sw_frame;
		} else
			tmp_frame = frame;

		size = av_image_get_buffer_size(
			(AVPixelFormat)tmp_frame->format, tmp_frame->width,
			tmp_frame->height, 1);
		buffer = (uint8_t *)av_malloc(size);
		if (!buffer) {
			fprintf(stderr, "Can not alloc buffer\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = av_image_copy_to_buffer(
			buffer, size, (const uint8_t *const *)tmp_frame->data,
			(const int *)tmp_frame->linesize,
			(AVPixelFormat)tmp_frame->format, tmp_frame->width,
			tmp_frame->height, 1);
		if (ret < 0) {
			fprintf(stderr, "Can not copy image to buffer\n");
			goto fail;
		}

		if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
			fprintf(stderr, "Failed to dump raw data.\n");
			goto fail;
		}

	fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
		av_freep(&buffer);
		if (ret < 0)
			return ret;
	}
}

int test()
{
	AVFormatContext *input_ctx = NULL;
	int video_stream, ret;
	AVStream *video = NULL;
	AVCodecContext *decoder_ctx = NULL;
	AVCodec *decoder = NULL;
	AVPacket packet;
	enum AVHWDeviceType type;
	int i;

	type = av_hwdevice_find_type_by_name("dxva2");
	if (type == AV_HWDEVICE_TYPE_NONE) {
		fprintf(stderr, "Device type %s is not supported.\n", "dxva2");
		fprintf(stderr, "Available device types:");
		while ((type = av_hwdevice_iterate_types(type)) !=
		       AV_HWDEVICE_TYPE_NONE)
			fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
		fprintf(stderr, "\n");
		return -1;
	}

	/* open the input file */
	if (avformat_open_input(&input_ctx, "E:\\test.mp4", NULL, NULL) != 0) {
		return -1;
	}

	if (avformat_find_stream_info(input_ctx, NULL) < 0) {
		fprintf(stderr, "Cannot find input stream information.\n");
		return -1;
	}

	/* find the video stream information */
	ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
				  &decoder, 0);
	if (ret < 0) {
		fprintf(stderr,
			"Cannot find a video stream in the input file\n");
		return -1;
	}
	video_stream = ret;

	for (i = 0;; i++) {
		const AVCodecHWConfig *config =
			avcodec_get_hw_config(decoder, i);
		if (!config) {
			fprintf(stderr,
				"Decoder %s does not support device type %s.\n",
				decoder->name, av_hwdevice_get_type_name(type));
			return -1;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
		    config->device_type == type) {
			hw_pix_fmt = config->pix_fmt;
			break;
		}
	}

	if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
		return AVERROR(ENOMEM);

	video = input_ctx->streams[video_stream];
	if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
		return -1;

	decoder_ctx->get_format = get_hw_format;

	if (hw_decoder_init(decoder_ctx, type) < 0)
		return -1;

	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
		fprintf(stderr, "Failed to open codec for stream #%u\n",
			video_stream);
		return -1;
	}

	/* open the file to dump raw data */
	output_file = fopen("E:\\cccc.264", "w+");

	/* actual decoding and dump the raw data */
	while (ret >= 0) {
		if ((ret = av_read_frame(input_ctx, &packet)) < 0)
			break;

		if (video_stream == packet.stream_index)
			ret = decode_write(decoder_ctx, &packet);

		av_packet_unref(&packet);
	}

	/* flush the decoder */
	packet.data = NULL;
	packet.size = 0;
	ret = decode_write(decoder_ctx, &packet);
	av_packet_unref(&packet);

	if (output_file)
		fclose(output_file);
	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&input_ctx);
	av_buffer_unref(&hw_device_ctx);
}


VideoDecoder::VideoDecoder(ScreenMirrorServer *s)
	: m_server(s)
{
	//test();
}

VideoDecoder::~VideoDecoder()
{
	uninitFFMPEG();
}

void VideoDecoder::docode(uint8_t *data, size_t data_len, bool is_key, uint64_t ts)
{
	int canOutput = -1;
	if (is_key) {
		if (m_pCodecCtx && data_len == m_pCodecCtx->extradata_size && memcmp(data, m_pCodecCtx->extradata, data_len) == 0)
			return;
		uninitFFMPEG();

		if (initFFMPEG(data, data_len) != 0)
			blog(LOG_ERROR, "decoder init fail!!!!!");

		return;
	} else {
		if (m_bCodecOpened) {
			AVPacket pkt1, *packet = &pkt1;
			AVFrame *tmp_frame = NULL;

			av_new_packet(packet, data_len);
			memcpy(packet->data, data, data_len);
			int ret =
				avcodec_send_packet(this->m_pCodecCtx, packet);
			if (ret == 0)
			{
				while (true)
				{
					AVFrame *pFrame = av_frame_alloc();
					AVFrame *sw_frame = av_frame_alloc();
					ret = avcodec_receive_frame(m_pCodecCtx, pFrame);
					if (ret == 0) {
						av_hwframe_transfer_data(sw_frame, pFrame, 0);
						sw_frame->pts = ts;
						m_server->outputVideo(sw_frame);
						av_frame_free(&pFrame);
					}
					else
					{
						av_frame_free(&pFrame);
						av_frame_free(&sw_frame);
						break;
					}
				}
			}
			av_packet_unref(packet);
		}
	}
}

int VideoDecoder::initFFMPEG(const void *privatedata, int privatedatalen)
{
	if (m_pCodec == NULL) {
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
		m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	}

	AVHWDeviceType type = av_hwdevice_find_type_by_name("dxva2");
	if (type == AV_HWDEVICE_TYPE_NONE) {
		fprintf(stderr, "Device type %s is not supported.\n", "dxva2");
		fprintf(stderr, "Available device types:");
		while ((type = av_hwdevice_iterate_types(type)) !=
		       AV_HWDEVICE_TYPE_NONE)
			fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
		fprintf(stderr, "\n");
		return -1;
	}

	for (int i = 0;; i++) {
		const AVCodecHWConfig *config =
			avcodec_get_hw_config(m_pCodec, i);
		if (!config) {
			fprintf(stderr,
				"Decoder %s does not support device type %s.\n",
				m_pCodec->name, av_hwdevice_get_type_name(type));
			return -1;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
		    config->device_type == type) {
			hw_pix_fmt = config->pix_fmt;
			break;
		}
	}

	if (!(m_pCodecCtx = avcodec_alloc_context3(m_pCodec)))
		return AVERROR(ENOMEM);

	m_pCodecCtx->get_format = get_hw_format;
	m_pCodecCtx->extradata = (uint8_t *)av_malloc(privatedatalen);
	m_pCodecCtx->extradata_size = privatedatalen;
	memcpy(m_pCodecCtx->extradata, privatedata, privatedatalen);

	if (hw_decoder_init(m_pCodecCtx, type) < 0)
		return -1;

	int ret = -1;
	if ((ret = avcodec_open2(m_pCodecCtx, m_pCodec, NULL)) < 0) {
		return -1;
	}

	m_bCodecOpened = true;

	return 0;
}

void VideoDecoder::uninitFFMPEG()
{
	if (m_pCodecCtx) {
		if (m_pCodecCtx->extradata) {
			av_freep(&m_pCodecCtx->extradata);
		}
		avcodec_free_context(&m_pCodecCtx);
		av_buffer_unref(&hw_device_ctx);
		m_pCodecCtx = NULL;
		m_pCodec = NULL;
		hw_device_ctx = NULL;
	}
	if (m_pSwsCtx) {
		sws_freeContext(m_pSwsCtx);
		m_pSwsCtx = NULL;
	}
}
