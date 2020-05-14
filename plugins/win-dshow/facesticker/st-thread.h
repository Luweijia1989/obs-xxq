#pragma once

#include <QThread>
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

struct FrameInfo
{
	FrameInfo(unsigned char *d, size_t s, long long t)
		: data(d)
		, size(s)
		, startTime(t)
		, avFrame(NULL) {}

	FrameInfo()
		: data(nullptr)
		, size(0)
		, startTime(0)
		, avFrame(NULL) {}
	unsigned char *data; // frame data should be cached
	size_t size;
	long long startTime;
	long long timestamp;
	AVFrame *avFrame;
	QString stickerId;
};

class DShowInput;
class STThread : public QThread
{
	Q_OBJECT
public:
	STThread(DShowInput *dsInput);
	~STThread();
	void setFrameConfig(const DShow::VideoConfig & cg);
	void setFrameConfig(int w, int h, AVPixelFormat f);
	bool stInited() { return m_stFunc->stInited(); }
	void addFrame(unsigned char *data, size_t size, long long startTime, QString id);
	void addFrame(AVFrame *frame, QString id);
	void stop();

protected:
	virtual void run() override;

private:
	bool InitGL();
	void unInitGL();
	void MesaOpenGL();
	GLboolean BindTexture(unsigned char *buffer, int width, int height, GLuint& texId);

	void processVideoData(unsigned char *buffer, size_t size, long long startTime, QString id);
	void processVideoData(AVFrame *frame, QString id);
	void processVideoDataInternal(AVFrame *frame, QString id);

private:
	DShowInput *m_dshowInput = nullptr;
	STFunction *m_stFunc = nullptr;
	bool m_running = false;
	moodycamel::BlockingReaderWriterQueue<FrameInfo> m_frameQueue;
	std::map<GLuint, std::vector<int> > gTextures;
	GLFWwindow* window = nullptr;
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
};
