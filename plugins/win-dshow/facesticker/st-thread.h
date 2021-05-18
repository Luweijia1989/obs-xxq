#pragma once

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QSet>
#include <QEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include "../libdshowcapture/dshowcapture.hpp"
#include "readerwriterqueue.h"
#include "st-function.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
}

class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;
class QOpenGLFramebufferObject;
class QOpenGLTexture;
class QOpenGLBuffer;

extern bool g_st_checkpass;

struct FrameInfo {
	long long startTime = 0;
	AVFrame *avFrame = NULL;
	int type = 0;
	int w;
	int h;
	int f;
};

class DShowInput;
class STThread : public QThread, protected QOpenGLFunctions {
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
	bool stInited() { return m_stFunc && m_stFunc->stInited() && g_st_checkpass; }
	bool needProcess();
	void updateInfo(const char *data);
	void updateSticker(const QString &stickerId, bool isAdd);
	void updateGameInfo(GameStickerType type, int region);
	void updateBeautifySetting(QString setting);

	void addFrame(unsigned char *data, size_t size, long long startTime, int w, int h);
	void addFrame(AVFrame *frame);
	void addConfigChangeFrame(int w, int h, int f);
	void setFrameConfig(int w, int h, int f);
	void setFrameConfig(const DShow::VideoConfig &cg);
	void quitThread();
	void freeResource();
	void setBeautifyEnabled(bool enabled);

protected:
	virtual void run() override;

private:
	void calcPosition(int &width, int &height);
	void initShader();
	void initOpenGLContext();
	void initVertexData();
	void initTexture();
	void processImage(AVFrame *frame, quint64 ts);
	void deleteTextures();
	void createTextures(int w, int h);
	void createPBO();
	void deletePBO();

private:
	moodycamel::BlockingReaderWriterQueue<FrameInfo> m_frameQueue;
	DShowInput *m_dshowInput = nullptr;
	STFunction *m_stFunc = nullptr;
	bool m_running = false;
	struct SwsContext *m_swsctx = NULL;
	struct SwsContext *m_swsctxYUV = NULL;
	AVPixelFormat m_curPixelFormat = AV_PIX_FMT_NONE;
	bool flip = false;
	int m_frameWidth = 0;
	int m_frameHeight = 0;
	bool m_videoFrameSizeChanged = false;
	AVFrame *m_swsRetFrame = nullptr;
	AVFrame *m_swsYUVRetFrame = nullptr;
	QMap<QString, int> m_stickers;
	QMutex m_stickerSetterMutex;
	GameStickerType m_gameStickerType = None;
	quint64 m_gameStartTime;
	int m_curRegion = -1;

	QOffscreenSurface *surface;
	QOpenGLContext *ctx;
	QOpenGLShaderProgram *m_shader = nullptr;
	QOpenGLVertexArrayObject *m_vao = nullptr;
	QOpenGLFramebufferObject *m_fbo = nullptr;
	QOpenGLTexture *m_backgroundTexture = nullptr;
	QOpenGLTexture *m_strawberryTexture = nullptr;
	QOpenGLTexture *m_bombTexture = nullptr;
	QOpenGLTexture *m_outputTexture = nullptr;
	QOpenGLTexture *m_beautify = nullptr;
	QOpenGLTexture *m_makeup = nullptr;
	QOpenGLTexture *m_filter = nullptr;
	quint64 m_textureBufferSize;
	QVector<QOpenGLBuffer*> m_pbos;
	QMutex m_beautifySettingMutex;
	QList<QString> m_beautifySettings;
	bool m_needBeautify = true;
};
