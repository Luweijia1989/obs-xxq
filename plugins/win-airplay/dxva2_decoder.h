#pragma once

#include <string>
#include <mutex>
#include <memory>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/hwcontext.h"
#include "libavutil/pixdesc.h"
}

class AVDecoder
{
public:
	AVDecoder& operator=(const AVDecoder&) = delete;
	AVDecoder(const AVDecoder&) = delete;
	AVDecoder();
	virtual ~AVDecoder();

	virtual bool Init(uint8_t *data, size_t len, void* d3d9_device);
	virtual void Destroy();
	bool AVDecoder::CheckSPSChanged(uint8_t *data, size_t len);

	virtual int  Send(AVPacket* packet);
	virtual int  Recv(AVFrame* frame);

private:
	std::mutex mutex_;

	AVCodecContext* codec_context_ = nullptr;
	AVDictionary* options_ = nullptr;
	AVBufferRef* device_buffer_ = nullptr;

	int decoder_reorder_pts_ = -1;
};

