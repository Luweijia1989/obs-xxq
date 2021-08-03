#include "VideoDecoder.h"
#include "airplay-server.h"
#include <obs.hpp>

enum AVHWDeviceType hw_priority[] = {
	AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2,
	AV_HWDEVICE_TYPE_VAAPI,   AV_HWDEVICE_TYPE_VDPAU,
	AV_HWDEVICE_TYPE_QSV,     AV_HWDEVICE_TYPE_NONE,
};

static bool has_hw_type(AVCodec *c, enum AVHWDeviceType type,
			enum AVPixelFormat *hw_format)
{
	for (int i = 0;; i++) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(c, i);
		if (!config) {
			break;
		}

		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
		    config->device_type == type) {
			*hw_format = config->pix_fmt;
			return true;
		}
	}

	return false;
}

void VideoDecoder::init_hw_decoder() {}

VideoDecoder::VideoDecoder(ScreenMirrorServer *s) : m_server(s) {}

VideoDecoder::~VideoDecoder()
{
	uninitFFMPEG();
}

void VideoDecoder::docode(uint8_t *data, size_t data_len, bool is_key,
			  uint64_t ts)
{
	int canOutput = -1;
	if (is_key) {
		if (m_pCodecCtx && data_len == m_pCodecCtx->extradata_size &&
		    memcmp(data, m_pCodecCtx->extradata, data_len) == 0)
			return;
		uninitFFMPEG();

		if (initFFMPEG(data, data_len) != 0)
			blog(LOG_ERROR, "decoder init fail!!!!!");

		return;
	} else {
		if (m_bCodecOpened) {
			AVPacket pkt1, *packet = &pkt1;
			int frameFinished;

			av_new_packet(packet, data_len);
			memcpy(packet->data, data, data_len);

			int ret =
				avcodec_send_packet(this->m_pCodecCtx, packet);
			frameFinished = avcodec_receive_frame(this->m_pCodecCtx,
							      hw_frame);

			av_packet_unref(packet);

			// Did we get a video frame?
			if (frameFinished == 0) {
				AVFrame *frame = av_frame_alloc();
				int err = av_hwframe_transfer_data(frame,
								   hw_frame, 0);
				if (err == 0)
					m_server->outputVideo(frame);
				else {
					av_frame_unref(frame);
					av_free(frame);
				}
			}
		}
	}
}

int VideoDecoder::initFFMPEG(const void *privatedata, int privatedatalen)
{
	if (m_pCodec == NULL) {
		m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
		m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	}
	if (m_pCodec == NULL) {
		return -1;
	}

	enum AVHWDeviceType *priority = hw_priority;
	AVBufferRef *hw_ctx = NULL;

	while (*priority != AV_HWDEVICE_TYPE_NONE) {
		if (has_hw_type(m_pCodec, *priority, &hw_format)) {
			int ret = av_hwdevice_ctx_create(&hw_ctx, *priority,
							 NULL, NULL, 0);
			if (ret == 0)
				break;
		}

		priority++;
	}

	if (hw_ctx) {
		m_pCodecCtx->hw_device_ctx = av_buffer_ref(hw_ctx);
		m_pCodecCtx->opaque = this;
		this->hw_ctx = hw_ctx;
	}

	hw_frame = av_frame_alloc();

	m_pCodecCtx->extradata = (uint8_t *)av_malloc(privatedatalen);
	m_pCodecCtx->extradata_size = privatedatalen;
	memcpy(m_pCodecCtx->extradata, privatedata, privatedatalen);
	//m_pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	int res = avcodec_open2(m_pCodecCtx, m_pCodec, NULL);
	if (res < 0) {

		printf("Failed to initialize decoder\n");
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

		if (hw_ctx) {
			av_buffer_unref(&hw_ctx);
		}

		if (hw_frame) {
			av_frame_unref(hw_frame);
			av_free(hw_frame);
		}

		avcodec_free_context(&m_pCodecCtx);
		m_pCodecCtx = NULL;
		m_pCodec = NULL;
	}
	if (m_pSwsCtx) {
		sws_freeContext(m_pSwsCtx);
		m_pSwsCtx = NULL;
	}
}
