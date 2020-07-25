#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/parseutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}


class VideoDecoder
{
public:
	VideoDecoder();
	~VideoDecoder();

	int docode(uint8_t *data, size_t data_len);

private:
	int initFFMPEG(const void *privatedata, int privatedatalen);
	void uninitFFMPEG();
private:
	AVCodec *m_pCodec = NULL;
	AVCodecContext *m_pCodecCtx = NULL;
	SwsContext *m_pSwsCtx = NULL;
	bool m_bCodecOpened = false;
};
