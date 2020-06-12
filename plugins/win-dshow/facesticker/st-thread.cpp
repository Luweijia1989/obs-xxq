#include "st-thread.h"
#include "obs.h"
#include <QDebug>
#include <QDateTime>
#include <QtMath>
#include <QFile>
#include <QElapsedTimer>
#include <QTimer>
#include <QRandomGenerator>
#include <QApplication>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include "..\win-dshow.h"

extern video_format ConvertVideoFormat(DShow::VideoFormat format);

bool g_st_checkpass = false;
#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)
#define G_VALUE 1000
#define STRAWBERRY_TIME 4
static void blend_image_rgba(struct VideoFrame *main,
			     struct VideoFrame *overlay, int x, int y)
{
	int real_overlay_height = 0;
	int real_overlay_width = 0;
	if (x >= 0) {
		real_overlay_width = main->width - x < overlay->width
					     ? main->width - x
					     : overlay->width;
	} else {
		real_overlay_width = main->width < overlay->width + x
					     ? main->width
					     : overlay->width + x;
	}

	if (y >= 0) {
		real_overlay_height = main->height - y < overlay->height
					      ? main->height - y
					      : overlay->height;
	} else {
		real_overlay_height = main->height < overlay->height + y
					      ? main->height
					      : overlay->height + y;
	}

	for (int j = 0; j < real_overlay_height; j++) {
		for (int i = 0; i < real_overlay_width; i++) {
			int overlay_pixel_pos = 0;
			int main_pixel_pos = 0;
			if (x >= 0) {
				if (y >= 0) {
					overlay_pixel_pos =
						j * overlay->width + i;
				} else {
					overlay_pixel_pos =
						(-y + j) * overlay->width + i;
				}
			} else {
				if (y >= 0) {
					overlay_pixel_pos =
						j * overlay->width + i - x;
				} else {
					overlay_pixel_pos =
						(-y + j) * overlay->width + i -
						x;
				}
			}
			if (x >= 0) {
				if (y >= 0) {
					main_pixel_pos =
						(y + j) * main->width + (x + i);
				} else {
					main_pixel_pos =
						j * main->width + (x + i);
				}
			} else {
				if (y >= 0) {
					main_pixel_pos =
						(y + j) * main->width + i;
				} else {
					main_pixel_pos = j * main->width + i;
				}
			}

			if (0) {
				uint8_t overlay_alpha =
					*(overlay->data +
					  overlay_pixel_pos * 4 + 3);
				if (overlay_alpha == 255) {
					main->data[main_pixel_pos * 4] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 2); //A
					main->data[main_pixel_pos * 4 + 1] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 1); //B
					main->data[main_pixel_pos * 4 + 2] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 0); //G
					main->data[main_pixel_pos * 4 + 3] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 3); //R
				} else if (overlay_alpha < 255 &&
					   overlay_alpha > 0) {
					main->data[main_pixel_pos * 4] = FAST_DIV255(
						main->data[main_pixel_pos * 4] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      2] *
							overlay_alpha); //A
					main->data[main_pixel_pos * 4 + 1] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   1] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      1] *
							overlay_alpha); //B
					main->data[main_pixel_pos * 4 + 2] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   2] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      0] *
							overlay_alpha); //G
					main->data[main_pixel_pos * 4 + 3] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   3] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      3] *
							overlay_alpha); //R
				}
			} else {
				uint8_t overlay_alpha =
					*(overlay->data +
					  overlay_pixel_pos * 4 + 3);
				if (overlay_alpha == 255) {
					main->data[main_pixel_pos * 4] =
						*(overlay->data +
						  overlay_pixel_pos * 4); //A
					main->data[main_pixel_pos * 4 + 1] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 1); //B
					main->data[main_pixel_pos * 4 + 2] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 2); //G
					main->data[main_pixel_pos * 4 + 3] = *(
						overlay->data +
						overlay_pixel_pos * 4 + 3); //R
				} else if (overlay_alpha < 255 &&
					   overlay_alpha > 0) {
					main->data[main_pixel_pos * 4] = FAST_DIV255(
						main->data[main_pixel_pos * 4] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
							      4] *
							overlay_alpha); //A
					main->data[main_pixel_pos * 4 + 1] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   1] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      1] *
							overlay_alpha); //B
					main->data[main_pixel_pos * 4 + 2] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   2] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      2] *
							overlay_alpha); //G
					main->data[main_pixel_pos * 4 + 3] = FAST_DIV255(
						main->data[main_pixel_pos * 4 +
							   3] *
							(255 - overlay_alpha) +
						overlay->data[overlay_pixel_pos *
								      4 +
							      3] *
							overlay_alpha); //R
				}
			}
		}
	}
}

enum AVPixelFormat obs_to_ffmpeg_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_NONE:
		return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_I444:
		return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_I420:
		return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12:
		return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YVYU:
		return AV_PIX_FMT_NONE;
	case VIDEO_FORMAT_YUY2:
		return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY:
		return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA:
		return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800:
		return AV_PIX_FMT_GRAY8;
	}

	return AV_PIX_FMT_NONE;
}

STThread::STThread(DShowInput *dsInput) : m_dshowInput(dsInput)
{
	m_stFunc = new STFunction;
	m_strawberryOverlay = QImage(":/mark/image/main/strawberry2.png");
	m_strawberryOverlay =
		m_strawberryOverlay.convertToFormat(QImage::Format_RGBA8888);
	m_bombOverlay = QImage(":/mark/image/main/bomb2.png");
	m_bombOverlay = m_bombOverlay.convertToFormat(QImage::Format_RGBA8888);
	m_strawberryFrameOverlay = {(size_t)m_strawberryOverlay.sizeInBytes(),
				    m_strawberryOverlay.width(),
				    m_strawberryOverlay.height(),
				    m_strawberryOverlay.bits()};
	m_bombFrameOverlay = {(size_t)m_bombOverlay.sizeInBytes(),
			      m_bombOverlay.width(), m_bombOverlay.height(),
			      m_bombOverlay.bits()};

	m_strawberryOverlayFlipV = m_strawberryOverlay.mirrored();
	m_strawberryFrameOverlayFlipV = {
		(size_t)m_strawberryOverlayFlipV.sizeInBytes(),
		m_strawberryOverlayFlipV.width(),
		m_strawberryOverlayFlipV.height(),
		m_strawberryOverlayFlipV.bits()};

	m_bombOverlayFlipV = m_bombOverlay.mirrored();
	m_bombFrameOverlayFlipV = {(size_t)m_bombOverlayFlipV.sizeInBytes(),
				   m_bombOverlayFlipV.width(),
				   m_bombOverlayFlipV.height(),
				   m_bombOverlayFlipV.bits()};
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
	m_running = true;
	if (!g_st_checkpass) {
		m_running = false;
		return;
	}

	if (!InitGL()) {
		qDebug() << "STThread fail to start, opengl init fail";
		m_running = false;
		return;
	}

	m_stFunc->initSenseTimeEnv();
	qDebug() << "SenseTime init result: " << m_stFunc->stInited();
	if (!m_stFunc->stInited()) {
		m_running = false;
		return;
	}

	while (m_running) {
		FrameInfo frame;
		m_frameQueue.wait_dequeue(frame);

		if (!m_running)
			break;

		//updateSticker();

		if (QDateTime::currentMSecsSinceEpoch() - frame.timestamp <
		    10) {
			if (frame.avFrame)
				processVideoData(frame.avFrame);

			if (frame.data)
				processVideoData(frame.data, frame.size,
						 frame.startTime);
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

void STThread::addFrame(unsigned char *data, size_t size, long long startTime)
{
	if (!m_running)
		return;
	FrameInfo info;
	info.size = size;
	info.startTime = startTime;
	info.data = (unsigned char *)bmalloc(size * sizeof(unsigned char *));
	memcpy(info.data, data, size);
	info.timestamp = QDateTime::currentMSecsSinceEpoch();
	m_frameQueue.enqueue(info);
}

void STThread::addFrame(AVFrame *frame)
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
	while (m_frameQueue.try_dequeue(info)) {
		if (info.avFrame)
			av_frame_free(&info.avFrame);

		if (info.data)
			bfree(info.data);
	}

	wait();
}

bool STThread::needProcess()
{
	QMutexLocker locker(&m_stickerSetterMutex);
	return !m_stickers.isEmpty() || m_gameStickerType != None;
}

void STThread::updateInfo(const char *data)
{
	QJsonDocument jd = QJsonDocument::fromJson(data);
	QJsonObject obj = jd.object();
	if (obj.isEmpty())
		return;

	int type = obj["type"].toInt(); //0 商汤贴纸 1 游戏
	if (type == 0) {
		updateSticker(obj["sticker"].toString(), obj["isAdd"].toBool());
	} else if (type == 1) {
		int gameType = obj["gameType"].toInt(); //1 草莓 0炸弹
		int region = obj["region"].toInt();
		updateGameInfo((GameStickerType)gameType, region);
	}
}

void STThread::updateGameInfo(GameStickerType type, int region)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	m_gameStickerType = type;
	if (type == Bomb || type == Strawberry) {
		m_curRegion = region;
		m_gameStartTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void STThread::updateSticker(const QString &stickerId, bool isAdd)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	if (isAdd) {
		if (m_stickers.contains(stickerId))
			return;
		int id = m_stFunc->addSticker(stickerId);
		m_stickers.insert(stickerId, id);
	} else {
		if (!m_stickers.contains(stickerId))
			return;

		m_stFunc->removeSticker(m_stickers.value(stickerId));
		m_stickers.remove(stickerId);
	}
}

void STThread::setFrameConfig(const DShow::VideoConfig &cg)
{
	video_format format = ConvertVideoFormat(cg.format);
	setFrameConfig(cg.cx, cg.cy, obs_to_ffmpeg_video_format(format));
}

void STThread::setFrameConfig(int w, int h, AVPixelFormat f)
{
	if (m_curFrameWidth != w || m_curFrameHeight != h ||
	    m_curPixelFormat != f) {
		if (m_swsRetFrame)
			av_frame_free(&m_swsRetFrame);
		m_swsRetFrame = av_frame_alloc();
		av_image_alloc(m_swsRetFrame->data, m_swsRetFrame->linesize, w,
			       h, AV_PIX_FMT_RGBA, 1);

		if (m_swsctx) {
			sws_freeContext(m_swsctx);
			m_swsctx = NULL;
		}

		if (m_stickerBuffer) {
			bfree(m_stickerBuffer);
			m_stickerBuffer = nullptr;
		}
		m_stickerBufferSize = sizeof(unsigned char) *
				      avpicture_get_size(AV_PIX_FMT_NV12, w, h);
		m_stickerBuffer = (unsigned char *)bmalloc(m_stickerBufferSize);

		m_curPixelFormat = f;
		flip = AV_PIX_FMT_BGRA == m_curPixelFormat;
		m_curFrameWidth = w;
		m_curFrameHeight = h;
		if (m_curPixelFormat != AV_PIX_FMT_NONE)
			m_swsctx =
				sws_getContext(w, h, m_curPixelFormat, w, h,
					       AVPixelFormat::AV_PIX_FMT_RGBA,
					       SWS_BICUBIC, NULL, NULL, NULL);
	}
}

void STThread::processVideoData(unsigned char *buffer, size_t size,
				long long startTime)
{
	if (!m_swsctx)
		return;

	AVFrame *tempFrame = av_frame_alloc();
	AVPicture *tempPicture = (AVPicture *)tempFrame;
	int ret = avpicture_fill(tempPicture, buffer, m_curPixelFormat,
				 m_curFrameWidth, m_curFrameHeight);
	tempFrame->pts = startTime;

	processVideoDataInternal(tempFrame);

	av_frame_free(&tempFrame);
}

void STThread::processVideoData(AVFrame *frame)
{
	setFrameConfig(frame->width, frame->height,
		       (AVPixelFormat)frame->format);

	if (!m_swsctx)
		return;

	if (!frame)
		return;
	processVideoDataInternal(frame);
}

void STThread::processVideoDataInternal(AVFrame *frame)
{
	int linesize = 0;
	int ret = sws_scale(m_swsctx, (const uint8_t *const *)(frame->data),
			    frame->linesize, 0, m_curFrameHeight,
			    m_swsRetFrame->data, m_swsRetFrame->linesize);
	if (flip)
		flipV();

	if (m_dshowInput->flipH)
		fliph();
	if (m_stFunc->doFaceDetect(m_swsRetFrame->data[0], m_curFrameWidth,
				   m_curFrameHeight)) {
		if (m_gameStickerType != None) {
			int s, h;
			calcPosition(s, h);
			qreal deltaTime = (QDateTime::currentMSecsSinceEpoch() -
					   m_gameStartTime) /
					  1000.;
			qreal gvalue =
				8 * h / (STRAWBERRY_TIME * STRAWBERRY_TIME);
			int s1 = s / qSqrt(8 * h / gvalue) * deltaTime;
			int h1 = qSqrt(2 * gvalue * h) * deltaTime -
				 0.5 * gvalue * deltaTime * deltaTime;
			QPoint center;
			if (flip)
				center = QPoint(s1 + m_curFrameWidth / 2 - 30,
						h1 + 30);
			else
				center = QPoint(s1 + m_curFrameWidth / 2 - 30,
						m_curFrameHeight - h1 - 30);

			QRect strawberryRect =
				QRect(center.x(), center.y(), 60, 60);

			bool hit = false;
			auto detectResult = m_stFunc->detectResult();
			if (detectResult.p_faces) {
				auto faceAction =
					detectResult.p_faces->face_action;
				if (m_gameStickerType == Strawberry) {
					bool mouseOpen = (faceAction &
							  ST_MOBILE_MOUTH_AH) ==
							 ST_MOBILE_MOUTH_AH;
					if (mouseOpen) {
						auto points =
							detectResult.p_faces
								->face106
								.points_array;

						QRect mouseRect = QRect(
							QPoint(points[84].x,
							       points[87].y),
							QPoint(points[90].x,
							       points[93].y));

						hit = strawberryRect.intersects(
							mouseRect);
					}
				} else if (m_gameStickerType == Bomb) {
					auto r = detectResult.p_faces->face106
							 .rect;
					QRect fr = QRect(QPoint(r.left, r.top),
							 QPoint(r.right,
								r.bottom));
					hit = fr.intersects(strawberryRect);
				}
			}

			QRect w(0, 0, m_curFrameWidth, m_curFrameHeight);
			if (hit || !w.intersects(strawberryRect)) {
				updateGameInfo(None, -1);
				qApp->postEvent(
					qApp,
					new QEvent((QEvent::Type)(
						hit ? QEvent::User + 1024
						    : QEvent::User + 1025)));
			}

			VideoFrame vf = {m_curFrameHeight * m_curFrameWidth * 4,
					 m_curFrameWidth, m_curFrameHeight,
					 m_swsRetFrame->data[0]};

			if (flip)
				blend_image_rgba(
					&vf,
					m_gameStickerType == Strawberry
						? &m_strawberryFrameOverlayFlipV
						: &m_bombFrameOverlayFlipV,
					center.x(), center.y());
			else
				blend_image_rgba(
					&vf,
					m_gameStickerType == Strawberry
						? &m_strawberryFrameOverlay
						: &m_bombFrameOverlay,
					center.x(), center.y());
		}

		BindTexture(m_swsRetFrame->data[0], m_curFrameWidth,
			    m_curFrameHeight, textureSrc);
		BindTexture(NULL, m_curFrameWidth, m_curFrameHeight,
			    textureDst);
		bool b = m_stFunc->doFaceSticker(
			textureSrc, textureDst, m_curFrameWidth,
			m_curFrameHeight, m_stickerBuffer, m_dshowInput->flipH);
		if (b)
			m_dshowInput->OutputFrame(false, false,
						  DShow::VideoFormat::NV12,
						  m_stickerBuffer,
						  m_stickerBufferSize,
						  frame->pts, 0);
	}
}

void STThread::calcPosition(int &width, int &height)
{
	if (m_curRegion == -1) {
		width = 0;
		height = 0;
	}

	int totalCount = 15;

	int stepx = m_curFrameWidth / 5;
	int stepy = m_curFrameHeight / 3;
	int x_r = m_curRegion % 5;
	int y_r = m_curRegion / 5;

	width = (x_r < 2 ? (x_r - 2.5) * stepx : (x_r - 1.5) * stepx);
	height = m_curFrameHeight - y_r * stepy;
}

void STThread::fliph()
{
	int y;
	int x;
	int l, r;
	char tmp;

	uchar *data = m_swsRetFrame->data[0];
	for (y = 0; y < m_curFrameHeight; ++y) {
		for (l = 0, r = m_curFrameWidth - 1; l < r; ++l, --r) {
			for (int d = 0; d < 4; ++d) {
				tmp = data[(y * m_curFrameWidth + l) * 4 + d];
				data[(y * m_curFrameWidth + l) * 4 + d] =
					data[(y * m_curFrameWidth + r) * 4 + d];
				data[(y * m_curFrameWidth + r) * 4 + d] = tmp;
			}
		}
	}
}

void STThread::flipV()
{
	int y;
	int x;
	int l, r;
	char tmp;

	uchar *data = m_swsRetFrame->data[0];
	for (y = 0; y < m_curFrameWidth; ++y) {
		for (l = 0, r = m_curFrameHeight - 1; l < r; ++l, --r) {
			for (int d = 0; d < 4; ++d) {
				tmp = data[(y + l * m_curFrameWidth) * 4 + d];
				data[(y + l * m_curFrameWidth) * 4 + d] =
					data[(y + m_curFrameWidth * r) * 4 + d];
				data[(y + m_curFrameWidth * r) * 4 + d] = tmp;
			}
		}
	}
}
