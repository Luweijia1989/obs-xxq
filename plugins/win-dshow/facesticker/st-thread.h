#pragma once

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QMap>
#include <QSet>
#include <QEvent>
#include "../libdshowcapture/dshowcapture.hpp"
#include "readerwriterqueue.h"
#include "st-function.h"

#ifdef WIN32
#include <GL/gl3w.h>
#else
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
}

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFunctions;

struct FrameInfo {
	FrameInfo(unsigned char *d, size_t s, long long t)
		: data(d), size(s), startTime(t), avFrame(NULL)
	{
	}

	FrameInfo() : data(nullptr), size(0), startTime(0), avFrame(NULL) {}
	unsigned char *data; // frame data should be cached
	size_t size;
	long long startTime;
	long long timestamp;
	AVFrame *avFrame;
};

struct VideoFrame {
	size_t frameSize;
	int width;
	int height;
	uchar *data;
};

class DShowInput;
class STThread : public QThread {
	Q_OBJECT
public:
	enum GameStickerType {
		Bomb,
		Strawberry,
		GameStart,
		GameStop,
		None,
	};
	STThread(DShowInput *dsInput);
	~STThread();
	void setFrameConfig(const DShow::VideoConfig &cg);
	void setFrameConfig(int w, int h, AVPixelFormat f);
	bool stInited() { return m_stFunc->stInited(); }
	void addFrame(unsigned char *data, size_t size, long long startTime);
	void addFrame(AVFrame *frame);
	void stop();
	bool needProcess();
	void updateInfo(const char *data);
	void updateSticker(const QString &stickerId, bool isAdd);
	void updateGameInfo(GameStickerType type, int region);

protected:
	virtual void run() override;

private:
	bool InitGL();
	void unInitGL();
	void MesaOpenGL();
	GLboolean BindTexture(unsigned char *buffer, int width, int height,
			      GLuint &texId);

	void processVideoData(unsigned char *buffer, size_t size,
			      long long startTime);
	void processVideoData(AVFrame *frame);
	void processVideoDataInternal(AVFrame *frame);
	void calcPosition(int &width, int &height);
	void fliph();

private:
	DShowInput *m_dshowInput = nullptr;
	STFunction *m_stFunc = nullptr;
	bool m_running = false;
	moodycamel::BlockingReaderWriterQueue<FrameInfo> m_frameQueue;
	std::map<GLuint, std::vector<int>> gTextures;
	GLFWwindow *window = nullptr;
	struct SwsContext *m_swsctx = NULL;
	AVPixelFormat m_curPixelFormat = AV_PIX_FMT_NONE;
	bool flip = false;
	int m_curFrameWidth = 0;
	int m_curFrameHeight = 0;
	AVFrame *m_swsRetFrame = nullptr;
	unsigned char *m_stickerBuffer = nullptr;
	size_t m_stickerBufferSize = 0;
	GLuint textureSrc = -1;
	GLuint textureDst = -1;
	QImage m_strawberryOverlay;
	QImage m_bombOverlay;
	QImage m_strawberryOverlayFlipV;
	QImage m_bombOverlayFlipV;
	VideoFrame m_strawberryFrameOverlay;
	VideoFrame m_bombFrameOverlay;
	VideoFrame m_strawberryFrameOverlayFlipV;
	VideoFrame m_bombFrameOverlayFlipV;
	QMap<QString, int> m_stickers;
	QMutex m_stickerSetterMutex;
	GameStickerType m_gameStickerType = None;
	quint64 m_gameStartTime;
	int m_curRegion = -1;
};
