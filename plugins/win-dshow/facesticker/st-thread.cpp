#include "st-thread.h"
#include "obs.h"
#include <QDebug>
#include <QDateTime>

extern video_format ConvertVideoFormat(DShow::VideoFormat format);

bool g_st_checkpass = false;

enum AVPixelFormat obs_to_ffmpeg_video_format(
	enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_NONE: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_I444: return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_I420: return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12: return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YVYU: return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_YUY2: return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY: return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA: return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX: return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800: return AV_PIX_FMT_GRAY8;
	}

	return AV_PIX_FMT_NONE;
}

STThread::STThread()
{
	m_stFunc = new STFunction;
}

STThread::~STThread()
{
	if (m_stickerBuffer)
		bfree(m_stickerBuffer);

	if (m_swsctx)
		sws_freeContext(m_swsctx);

	if (m_swsRetFrame)
		av_frame_free(&m_swsRetFrame);

	m_stFunc->freeFaceHandler();
	delete m_stFunc;
}

void STThread::run()
{
	if (!g_st_checkpass)
		return;

	if (!InitGL()) {
		qDebug() << "STThread fail to start, opengl init fail";
		m_running = false;
		return;
	}

	m_stFunc->initSenseTimeEnv();
	qDebug() << "SenseTime init result: " << m_stFunc->stInited();
	if (!m_stFunc->stInited())
		return;
	
	m_running = true;
	while (m_running)
	{
		FrameInfo frame;
		m_frameQueue.wait_dequeue(frame);

		if (!m_running)
			break;

		if (QDateTime::currentMSecsSinceEpoch() - frame.timestamp < 10) {
			if (frame.avFrame)
				processVideoData(frame.avFrame, frame.stickerId);

			if (frame.data)
				processVideoData(frame.data, frame.size, frame.startTime, frame.stickerId);
		}

		if (frame.data)
			bfree(frame.data);

		if (frame.avFrame)
			av_frame_free(&frame.avFrame);
	}

	m_stFunc->freeSticker();

	unInitGL();

	qDebug() << "STThread stopped...";
}

void STThread::addFrame(unsigned char *data, size_t size, long long startTime, QString id)
{
	if (!m_running)
		return;
	FrameInfo info;
	info.stickerId = id;
	info.size = size;
	info.startTime = startTime;
	info.data = (unsigned char *)bmalloc(size * sizeof(unsigned char *));
	memcpy(info.data, data, size);
	info.timestamp = QDateTime::currentMSecsSinceEpoch();
	m_frameQueue.enqueue(info);
}

void STThread::addFrame(AVFrame *frame, QString id)
{
	if (!m_running)
		return;
	AVFrame *copyFrame = av_frame_alloc();
	copyFrame->format = frame->format;
	copyFrame->width = frame->width;
	copyFrame->height = frame->height;
	copyFrame->channels = frame->channels;
	copyFrame->channel_layout = frame->channel_layout;
	copyFrame->nb_samples = frame->nb_samples;
	av_frame_get_buffer(copyFrame, 32);
	av_frame_copy(copyFrame, frame);
	av_frame_copy_props(copyFrame, frame);

	FrameInfo info;
	info.stickerId = id;
	info.avFrame = copyFrame;
	info.timestamp = QDateTime::currentMSecsSinceEpoch();
	m_frameQueue.enqueue(info);
}

void STThread::stop()
{
	if (!m_running)
		return;

	m_running = false;
	m_frameQueue.enqueue(FrameInfo());

	FrameInfo info;
	while (m_frameQueue.try_dequeue(info))
	{
		if (info.avFrame)
			av_frame_free(&info.avFrame);

		if (info.data)
			bfree(info.data);
	}

	wait();
}

void STThread::setFrameConfig(const DShow::VideoConfig & cg)
{
	video_format format = ConvertVideoFormat(cg.format);
	setFrameConfig(cg.cx, cg.cy, obs_to_ffmpeg_video_format(format));
}

void STThread::setFrameConfig(int w, int h, AVPixelFormat f)
{
	if (m_curFrameWidth != w || m_curFrameHeight != h || m_curPixelFormat != f) {
		if (m_swsRetFrame)
			av_frame_free(&m_swsRetFrame);
		m_swsRetFrame = av_frame_alloc();
		av_image_alloc(m_swsRetFrame->data, m_swsRetFrame->linesize, w, h, AV_PIX_FMT_RGBA, 1);

		if (m_swsctx) {
			sws_freeContext(m_swsctx);
			m_swsctx = NULL;
		}

		if (m_stickerBuffer) {
			bfree(m_stickerBuffer);
			m_stickerBuffer = nullptr;
		}
		m_stickerBufferSize = sizeof(unsigned char)*avpicture_get_size(AV_PIX_FMT_YUV420P, w, h);
		m_stickerBuffer = (unsigned char *)bmalloc(m_stickerBufferSize);

		m_curPixelFormat = f;
		flip = AV_PIX_FMT_BGRA == m_curPixelFormat;
		m_curFrameWidth = w;
		m_curFrameHeight = h;
		if (m_curPixelFormat != AV_PIX_FMT_NONE)
			m_swsctx = sws_getContext(w,
				h,
				m_curPixelFormat,
				w,
				h,
				AVPixelFormat::AV_PIX_FMT_RGBA,
				SWS_BICUBIC,
				NULL,
				NULL,
				NULL);
	}
}



void STThread::processVideoData(unsigned char *buffer, size_t size, long long startTime, QString id)
{
	if (!m_swsctx)
		return;

	AVFrame *tempFrame = av_frame_alloc();
	AVPicture *tempPicture = (AVPicture *)tempFrame;
	int ret = avpicture_fill(tempPicture, buffer, m_curPixelFormat, m_curFrameWidth, m_curFrameHeight);
	tempFrame->pts = startTime;

	processVideoDataInternal(tempFrame, id);

	av_frame_free(&tempFrame);
}

void STThread::processVideoData(AVFrame *frame, QString id)
{
	setFrameConfig(frame->width, frame->height, (AVPixelFormat)frame->format);

	if (!m_swsctx)
		return;

	if (!frame)
		return;
	processVideoDataInternal(frame, id);
}
