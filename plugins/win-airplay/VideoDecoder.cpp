#include "VideoDecoder.h"
#include "airplay-server.h"
#include <obs.hpp>

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
			AVFrame *pFrame;

			pFrame = av_frame_alloc();

			av_new_packet(packet, data_len);
			memcpy(packet->data, data, data_len);

			int ret =
				avcodec_send_packet(this->m_pCodecCtx, packet);
			frameFinished = avcodec_receive_frame(this->m_pCodecCtx,
							      pFrame);

			av_packet_unref(packet);

			// Did we get a video frame?
			if (frameFinished == 0) {
				pFrame->pts = ts;
				m_server->outputVideo(pFrame);
			} else
				av_frame_free(&pFrame);
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

	m_pCodecCtx->extradata = (uint8_t *)av_malloc(privatedatalen);
	m_pCodecCtx->extradata_size = privatedatalen;
	memcpy(m_pCodecCtx->extradata, privatedata, privatedatalen);
	m_pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
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
		avcodec_free_context(&m_pCodecCtx);
		m_pCodecCtx = NULL;
		m_pCodec = NULL;
	}
	if (m_pSwsCtx) {
		sws_freeContext(m_pSwsCtx);
		m_pSwsCtx = NULL;
	}
}
