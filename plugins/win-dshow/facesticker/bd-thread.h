#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariant>
#include "../libdshowcapture/dshowcapture.hpp"
#include "readerwriterqueue.h"
#include "effect_manager/be_effect_handle.h"
#include "effect_manager/be_render.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
}

class DShowInput;
class PBOReader;

namespace BEF {
class BEFEffectGLContext;
}

class BDThread : public QThread {
	Q_OBJECT
public:
	struct FrameInfo {
		long long startTime = 0;
		AVFrame *avFrame = NULL;
	};

	enum GameStickerType {
		Bomb,
		Strawberry,
		GameStart,
		GameStop,
		None,
	};
	BDThread(DShowInput *dsInput);
	~BDThread();
	bool stInited() { return effectHandlerInited; }
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
	void initShader();
	void initVertexData();
	void processImage(AVFrame *frame, quint64 ts, BEF::BEFEffectGLContext *ctx);
	void deleteTextures();
	void createTextures(int w, int h);
	void ensureCacheBuffer(size_t size);
	void createPBO(int w, int h);
	void deletePBO();

private:
	QMutex waitMutex;
	QWaitCondition waitCondition;

	moodycamel::BlockingReaderWriterQueue<FrameInfo> m_frameQueue;
	DShowInput *m_dshowInput = nullptr;
	bool effectHandlerInited = false;
	EffectHandle *m_stFunc = nullptr;
	
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

	GLuint m_shader;
	GLuint m_vao;
	GLuint m_vbo;
	GLuint m_fbo;
	PBOReader *m_reader = nullptr;
	GLuint m_backgroundTexture = 0;
	GLuint m_outputTexture = 0;
	GLuint m_outputTexture2 = 0;
	QMutex m_beautifySettingMutex;
	QList<QString> m_beautifySettings;
	bool m_needBeautify = true;
	unsigned char *m_dataBuffer = nullptr;
	long long m_dataBufferSize = 0;
	int m_lastWidth = 0;
	int m_lastHeight = 0;
	int m_lastFormat = -1;
};
