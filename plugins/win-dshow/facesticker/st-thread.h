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
class QOpenGLShader;

extern bool g_st_checkpass;

struct FrameInfo {
	long long startTime = 0;
	AVFrame *avFrame = NULL;
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
	void updateInfo(const char *data);
	void updateSticker(const QString &stickerId, bool isAdd);
	void updateGameInfo(GameStickerType type, int region);
	void updateBeautifySetting(QString setting);

	void addFrame(unsigned char *data, size_t size, long long startTime, int w, int h, int f);
	void addFrame(AVFrame *frame, long long startTime);
	void quitThread();
	void freeResource();
	void setBeautifyEnabled(bool enabled);

	void waitStarted();

protected:
	virtual void run() override;

private:
	void calcPosition(int &width, int &height, int w, int h);
	void initShader();
	void initOpenGLContext();
	void initVertexData();
	void initTexture();
	void processImage(AVFrame *frame, quint64 ts);
	void deleteTextures();
	void createTextures(int w, int h);
	void createPBO(quint64 size);
	void deletePBO();
	void copyTexture(QOpenGLTexture *texture);
	void ensureCacheBuffer(size_t size);

private:
	QMutex waitMutex;
	QWaitCondition waitCondition;

	moodycamel::BlockingReaderWriterQueue<FrameInfo> m_frameQueue;
	DShowInput *m_dshowInput = nullptr;
	STFunction *m_stFunc = nullptr;
	bool m_running = false;
	struct SwsContext *m_swsctx = NULL;
	bool flip = false;
	uint8_t *m_outDataBuffer = nullptr;
	AVFrame *m_cacheFrame = nullptr;
	QMap<QString, int> m_stickers;
	QRecursiveMutex m_stickerSetterMutex;
	GameStickerType m_gameStickerType = None;
	quint64 m_gameStartTime;
	int m_curRegion = -1;

	QOffscreenSurface *surface;
	QOpenGLContext *ctx;
	QOpenGLShaderProgram *m_shader = nullptr;
	QOpenGLShaderProgram *m_flipShader = nullptr;
	QOpenGLShader *m_vertexShader = nullptr;
	QOpenGLShader *m_vertexShader2 = nullptr;
	QOpenGLShader *m_fragmentShader = nullptr;
	QOpenGLShader *m_fragmentShader2 = nullptr;
	QOpenGLVertexArrayObject *m_vao = nullptr;
	QOpenGLFramebufferObject *m_fbo = nullptr;
	QOpenGLTexture *m_backgroundTexture = nullptr;
	QOpenGLTexture *m_strawberryTexture = nullptr;
	QOpenGLTexture *m_bombTexture = nullptr;
	QOpenGLTexture *m_outputTexture = nullptr;
	QOpenGLTexture *m_beautify = nullptr;
	QOpenGLTexture *m_makeup = nullptr;
	QOpenGLTexture *m_filter = nullptr;
	QVector<QOpenGLBuffer*> m_pbos;
	QMutex m_beautifySettingMutex;
	QList<QString> m_beautifySettings;
	bool m_needBeautify = true;
	unsigned char *m_dataBuffer = nullptr;
	long long m_dataBufferSize = 0;
	int m_lastWidth = 0;
	int m_lastHeight = 0;
	int m_lastFormat = -1;
};
