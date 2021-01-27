#pragma once

#include <QThread>
#include <QImage>
#include <QMutex>
#include <QMap>
#include <QSet>
#include <QEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include "../libdshowcapture/dshowcapture.hpp"
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
	void setFrameConfig(const DShow::VideoConfig &cg);
	void setFrameConfig(int w, int h, AVPixelFormat f);
	bool stInited() { return m_stFunc->stInited(); }
	bool needProcess();
	void updateInfo(const char *data);
	void updateSticker(const QString &stickerId, bool isAdd);
	void updateGameInfo(GameStickerType type, int region);

	Q_INVOKABLE void processImage(uint8_t **data, int *linesize);

protected:
	virtual void run() override;

private:
	void calcPosition(int &width, int &height);
	void initShader();
	void initOpenGLContext();
	void initVertexData();
	void initTexture();

private:
	DShowInput *m_dshowInput = nullptr;
	STFunction *m_stFunc = nullptr;
	bool m_running = false;
	std::map<GLuint, std::vector<int>> gTextures;
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
	int m_fboWidth = 0;
	int m_fboHeight = 0;
};
