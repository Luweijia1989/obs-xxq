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

class AVDecoder {
public:
	AVDecoder &operator=(const AVDecoder &) = delete;
	AVDecoder(const AVDecoder &) = delete;
	AVDecoder();
	virtual ~AVDecoder();

	virtual void Init(uint8_t *data, size_t len, void *d3d9_device, bool useSoftware);
	virtual void Destroy();
	bool AVDecoder::CheckSPSChanged(uint8_t *data, size_t len);

	virtual int Send(AVPacket *packet);
	virtual int Recv(AVFrame *frame);

	bool IsHWDecode() { return is_hw_decode; }

private:
	bool initHwDecode(uint8_t *data, size_t len, void *d3d9_device);
	bool initSoftDecode(uint8_t *data, size_t len);

private:
	std::mutex mutex_;

	AVCodecContext *codec_context_ = nullptr;
	AVDictionary *options_ = nullptr;
	AVBufferRef *device_buffer_ = nullptr;

	int decoder_reorder_pts_ = -1;
	bool is_hw_decode = false;
	bool is_recv_first_frame = false;
};
