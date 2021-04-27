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
#include <QDebug>
#include <QOpenGLFramebufferObject>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLExtraFunctions>
#include "..\win-dshow.h"

extern video_format ConvertVideoFormat(DShow::VideoFormat format);

bool g_st_checkpass = false;
#define G_VALUE 1000
#define STRAWBERRY_TIME 4

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
	surface = new QOffscreenSurface(nullptr, this);
	surface->create();
	m_stFunc = new STFunction;
	moveToThread(this);
}

STThread::~STThread()
{

}

void STThread::run()
{
	m_running = true;
	if (!g_st_checkpass) {
		freeResource();
		m_running = false;
		return;
	}

	initOpenGLContext();
	initShader();
	initVertexData();
	initTexture();

	m_stFunc->initSenseTimeEnv();
	qDebug() << "SenseTime init result: " << m_stFunc->stInited();
	if (!m_stFunc->stInited()) {
		freeResource();
		m_running = false;
		return;
	}

	while (m_running)
	{
		m_producerMutex.lock();
		m_producerCondition.wait(&m_producerMutex);
		if (m_running)
			processImage(m_data, m_linesize, m_ts);
		m_producerMutex.unlock();

		m_consumerMutex.lock();
		m_consumerCondition.notify_one();
		m_consumerMutex.unlock();
	}

	ctx->makeCurrent(surface);
	delete m_fbo;
	deleteTextures();
	m_strawberryTexture->destroy();
	delete m_strawberryTexture;
	m_bombTexture->destroy();
	delete m_bombTexture;

	deletePBO();

	delete m_shader;
	delete m_vao;
	delete ctx;
	freeResource();
	qDebug() << "STThread stopped...";
}

bool STThread::needProcess()
{
	return true;
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

void STThread::videoDataReceived(uint8_t **data, int *linesize, quint64 ts)
{
	m_producerMutex.lock();
	m_data = data;
	m_linesize = linesize;
	m_ts = ts;
	m_producerCondition.notify_one();
	m_producerMutex.unlock();

	m_consumerMutex.lock();
	m_consumerCondition.wait(&m_consumerMutex);
	m_consumerMutex.unlock();
}

void STThread::processImage(uint8_t **data, int *linesize, quint64 ts)
{
	bool needMask = m_gameStickerType != None;
	bool needSticker = !m_stickers.isEmpty();

	int ret = sws_scale(m_swsctx, (const uint8_t *const *)(data),
			    (const int *)linesize, 0, m_frameHeight,
			    m_swsRetFrame->data, m_swsRetFrame->linesize);

	ctx->makeCurrent(surface);
	if (m_videoFrameSizeChanged) {
		if (m_fbo)
			delete m_fbo;

		m_fbo = new QOpenGLFramebufferObject(m_frameWidth, m_frameHeight);

		deleteTextures();

		m_backgroundTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
		m_backgroundTexture->setSize(m_frameWidth, m_frameHeight);
		m_backgroundTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
		m_backgroundTexture->allocateStorage();

		createTextures(m_frameWidth, m_frameHeight);

		m_textureBufferSize = m_frameWidth * m_frameHeight * 4;

		deletePBO();
		createPBO();
	}

	m_videoFrameSizeChanged = false;

	m_backgroundTexture->setData(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8,m_swsRetFrame->data[0]);

	m_stFunc->doFaceDetect(m_swsRetFrame->data[0], m_frameWidth,  m_frameHeight, flip);

	QOpenGLTexture *nextSrc = m_backgroundTexture;
	if (1) {//是否美颜
		nextSrc = m_stFunc->doBeautify(m_backgroundTexture, m_beautify, m_makeup, m_filter, m_frameWidth, m_frameHeight, m_dshowInput->flipH, flip);
	}
	if (needSticker)
		m_stFunc->doFaceSticker(nextSrc->textureId(), m_outputTexture->textureId(), m_frameWidth, m_frameHeight, m_dshowInput->flipH, flip);

	m_stFunc->flipFaceDetect(flip, m_dshowInput->flipH, m_frameWidth, m_frameHeight); // 翻转得到实际的人脸关键点信息

	float maskX = 0.;
	float maskY = 0.;
	float maskWidth = (float)m_strawberryTexture->width() / m_frameWidth * 2;
	float maskHeight = -(float)m_strawberryTexture->height() / m_frameHeight * 2;
	if (needMask) {
		int s, h;
		calcPosition(s, h);
		qreal deltaTime = (QDateTime::currentMSecsSinceEpoch() - m_gameStartTime) / 1000.;
		qreal gvalue = 8 * h / (STRAWBERRY_TIME * STRAWBERRY_TIME);
		int s1 = s / qSqrt(8 * h / gvalue) * deltaTime;
		int h1 = qSqrt(2 * gvalue * h) * deltaTime - 0.5 * gvalue * deltaTime * deltaTime;
		QPoint center = QPoint(s1 + m_frameWidth / 2 - m_strawberryTexture->width() / 2, m_strawberryTexture->height() + h1);
		QRect strawberryRect = QRect(center.x(), m_frameHeight - center.y(), m_strawberryTexture->width(), m_strawberryTexture->height());
		maskX = 2. * center.x() / m_frameWidth - 1;
		maskY = 2. * center.y() / m_frameHeight - 1;
	
		bool hit = false;
		auto detectResult = m_stFunc->detectResult();
		if (detectResult.p_faces) {
			auto faceAction = detectResult.p_faces->face_action;
			if (m_gameStickerType == Strawberry) {
				bool mouseOpen = (faceAction & ST_MOBILE_MOUTH_AH) == ST_MOBILE_MOUTH_AH;
				if (mouseOpen) {
					auto points = detectResult.p_faces ->face106.points_array;
					QRect mouseRect = QRect(QPoint(points[84].x, points[87].y), QPoint(points[90].x, points[93].y));
					hit = strawberryRect.intersects(mouseRect);
				}
			} else if (m_gameStickerType == Bomb) {
				auto r = detectResult.p_faces->face106.rect;
				QRect fr = QRect(QPoint(r.left, r.top), QPoint(r.right, r.bottom));
				hit = fr.intersects(strawberryRect);
			}
		}
	
		QRect w(0, 0, m_frameWidth, m_frameHeight);
		if (hit || !w.intersects(strawberryRect)) {
			updateGameInfo(None, -1);
			needMask = false;
			qApp->postEvent(qApp, new QEvent((QEvent::Type)(hit ? QEvent::User + 1024 : QEvent::User + 1025)));
		}
	}

	m_fbo->bind();

	glActiveTexture(GL_TEXTURE0);
	if (needSticker)
		m_outputTexture->bind();
	else
		nextSrc->bind();

	glActiveTexture(GL_TEXTURE1);
	if (m_gameStickerType == Strawberry)
		m_strawberryTexture->bind();
	else if (m_gameStickerType == Bomb)
		m_bombTexture->bind();

	glViewport(0, 0, m_frameWidth, m_frameHeight);
	m_vao->bind();
	m_shader->bind();
	{
		QMatrix4x4 model;
		model.scale(1, -1);

		QMatrix4x4 flipMatrix;

		if (m_dshowInput->flipH) {
			model.scale(-1, 1);
			flipMatrix.scale(-1, 1);
			maskX = -maskX - maskWidth;
		}

		if (flip) {
			model.scale(1, -1);
			flipMatrix.scale(1, -1);
			maskY = -maskY - maskHeight;
		}

		m_shader->setUniformValue("needMask", needMask);
		m_shader->setUniformValue("flipMatrix", flipMatrix);
		m_shader->setUniformValue("model", model);
		m_shader->setUniformValue("leftTop", QVector2D(maskX, maskY));
		m_shader->setUniformValue("maskSize",
					  QVector2D(maskWidth, maskHeight));
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	static int index = 0;
	int nextIndex = 0; // pbo index used for next frame
	index = (index + 1) % 2;
	nextIndex = (index + 1) % 2;

	QOpenGLExtraFunctions *extraFuncs = ctx->extraFunctions();
	extraFuncs->glReadBuffer(GL_COLOR_ATTACHMENT0);

	m_pbos[index]->bind();
	glReadPixels(0, 0, m_frameWidth, m_frameHeight, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	m_pbos[index]->release();
	m_pbos[nextIndex]->bind();
	auto src = m_pbos[nextIndex]->map(QOpenGLBuffer::ReadOnly);
	if (src) {
		m_dshowInput->OutputFrame(false, false, DShow::VideoFormat::ARGB, (unsigned char *)src, m_textureBufferSize, ts, 0);
		m_pbos[nextIndex]->unmap();
	}
	m_pbos[nextIndex]->release();

	m_vao->release();
	m_shader->release();
	m_fbo->release();

	ctx->doneCurrent();
}

void STThread::deleteTextures()
{
	if (m_backgroundTexture) {
		m_backgroundTexture->destroy();
		delete m_backgroundTexture;
		m_backgroundTexture = nullptr;
	}
	if (m_outputTexture) {
		m_outputTexture->destroy();
		delete m_outputTexture;
		m_outputTexture = nullptr;
	}
	if (m_beautify) {
		m_beautify->destroy();
		delete m_beautify;
		m_beautify = nullptr;
	}
	if (m_makeup) {
		m_makeup->destroy();
		delete m_makeup;
		m_makeup = nullptr;
	}
	if (m_filter) {
		m_filter->destroy();
		delete m_filter;
		m_filter = nullptr;
	}
}

void STThread::createTextures(int w, int h)
{
	m_outputTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_outputTexture->setSize(m_frameWidth, m_frameHeight);
	m_outputTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_outputTexture->allocateStorage();

	m_beautify = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_beautify->setSize(m_frameWidth, m_frameHeight);
	m_beautify->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_beautify->allocateStorage();

	m_makeup = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_makeup->setSize(m_frameWidth, m_frameHeight);
	m_makeup->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_makeup->allocateStorage();

	m_filter = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_filter->setSize(m_frameWidth, m_frameHeight);
	m_filter->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_filter->allocateStorage();
}

void STThread::createPBO()
{
	for (int i=0; i<2; i++)
	{
		QOpenGLBuffer *pbo = new QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer);
		pbo->create();
		pbo->bind();
		pbo->allocate(m_textureBufferSize);
		m_pbos.append(pbo);
	}
}

void STThread::deletePBO()
{
	if (m_pbos.isEmpty())
		return;

	for (int i = 0; i < 2; i++) {
		m_pbos[i]->destroy();
		delete m_pbos[i];
	}
	m_pbos.clear();
}

void STThread::updateSticker(const QString &stickerId, bool isAdd)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	if (isAdd) {
		if (m_stickers.contains(stickerId))
			return;
		int id = m_stFunc->addSticker(stickerId);
		qDebug() << "st do sticker, sticker id: " << stickerId;
		m_stickers.insert(stickerId, id);
	} else {
		if (!m_stickers.contains(stickerId))
			return;

		qDebug() << "st remove sticker, sticker id: " << stickerId;
		m_stFunc->removeSticker(m_stickers.value(stickerId));
		m_stickers.remove(stickerId);
	}
}

void STThread::setFrameConfig(const DShow::VideoConfig &cg)
{
	video_format format = ConvertVideoFormat(cg.format);
	setFrameConfig(cg.cx, cg.cy, obs_to_ffmpeg_video_format(format));
}

void STThread::setFrameConfig(int w, int h, int f)
{
	if (m_frameWidth != w || m_frameHeight != h || m_curPixelFormat != f) {
		if (m_frameWidth != w || m_frameHeight != h)
			m_videoFrameSizeChanged = true;

		if (m_swsRetFrame)
			av_frame_free(&m_swsRetFrame);
		m_swsRetFrame = av_frame_alloc();
		av_image_alloc(m_swsRetFrame->data, m_swsRetFrame->linesize, w,
			       h, AV_PIX_FMT_RGBA, 1);

		if (m_swsctx) {
			sws_freeContext(m_swsctx);
			m_swsctx = NULL;
		}

		m_curPixelFormat = (AVPixelFormat)f;
		flip = AV_PIX_FMT_BGRA == m_curPixelFormat;
		m_frameWidth = w;
		m_frameHeight = h;
		if (m_curPixelFormat != AV_PIX_FMT_NONE)
			m_swsctx = sws_getContext(w, h, m_curPixelFormat, w, h, AVPixelFormat::AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);
	}
}

void STThread::quitThread()
{
	m_running = false;
	m_producerMutex.lock();
	m_producerCondition.notify_one();
	m_producerMutex.unlock();
	wait();
}

void STThread::freeResource()
{
	if (m_swsctx)
		sws_freeContext(m_swsctx);

	if (m_swsRetFrame)
		av_frame_free(&m_swsRetFrame);

	m_stFunc->freeStResource();
	delete m_stFunc;
}

void STThread::calcPosition(int &width, int &height)
{
	if (m_curRegion == -1) {
		width = 0;
		height = 0;
	}

	int totalCount = 15;

	int stepx = m_frameWidth / 5;
	int stepy = m_frameHeight / 3;
	int x_r = m_curRegion % 5;
	int y_r = m_curRegion / 5;

	width = (x_r < 2 ? (x_r - 2.5) * stepx : (x_r - 1.5) * stepx);
	height = m_frameHeight - (y_r + 0.5) * stepy;
}

void STThread::initShader()
{
	auto m_vertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
	auto m_fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment);
	bool b = m_vertexShader->compileSourceCode(R"(
                                               #version 330 core
                                               layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

                                               out vec2 TexCoords;
                                               out vec4 mainPosition;

                                               uniform mat4 model;
                                               uniform mat4 projection;

                                               void main()
                                               {
                                                   TexCoords = vertex.zw;
                                                   mainPosition = vec4(vertex.xy, 0.0, 1.0);
                                                   gl_Position = model * mainPosition;
                                               }
                                               )");

	b = m_fragmentShader->compileSourceCode(R"(
                                            #version 330 core
                                            in vec2 TexCoords;
                                            in vec4 mainPosition;
                                            out vec4 color;

                                            uniform sampler2D image;
                                            uniform sampler2D maskImage;
                                            uniform vec2 leftTop;
                                            uniform vec2 maskSize;
                                            uniform mat4 flipMatrix;
					    uniform bool needMask;
                                            void main()
                                            {
                                                vec4 imageColor = vec4(texture(image, TexCoords).rgb, 1);

                                                if (needMask && mainPosition.x >= leftTop.x && mainPosition.y <= leftTop.y && mainPosition.x <= leftTop.x + maskSize.x && mainPosition.y >= leftTop.y + maskSize.y) {
                                                    vec4 maskCoords = flipMatrix * vec4((mainPosition.x - leftTop.x) / maskSize.x, (mainPosition.y - leftTop.y) / maskSize.y, 1.0, 1.0);
                                                    vec4 maskColor = texture(maskImage, maskCoords.xy);

                                                    vec4 outputColor;
                                                    float a = maskColor.a + imageColor.a * (1.0 - maskColor.a);
                                                    float alphaDivisor = a + step(a, 0.0);
                                                    outputColor.r = (maskColor.r * maskColor.a + imageColor.r * imageColor.a * (1.0 - maskColor.a))/alphaDivisor;
                                                    outputColor.g = (maskColor.g * maskColor.a + imageColor.g * imageColor.a * (1.0 - maskColor.a))/alphaDivisor;
                                                    outputColor.b = (maskColor.b * maskColor.a + imageColor.b * imageColor.a * (1.0 - maskColor.a))/alphaDivisor;
                                                    outputColor.a = a;
                                                    color = outputColor;
                                                }
                                                else {
                                                    color = imageColor;
                                                }
                                            }
                                            )");
	m_shader = new QOpenGLShaderProgram;
	m_shader->addShader(m_vertexShader);
	m_shader->addShader(m_fragmentShader);
	m_shader->link();

	m_shader->bind();
	m_shader->setUniformValue("image", 0);
	m_shader->setUniformValue("maskImage", 1);
	m_shader->release();
}

void STThread::initOpenGLContext()
{
	ctx = new QOpenGLContext;
	ctx->create();
	ctx->makeCurrent(surface);
	initializeOpenGLFunctions();

	glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA,
			    GL_ONE_MINUS_SRC_ALPHA);
}

void STThread::initVertexData()
{
	m_vao = new QOpenGLVertexArrayObject;
	m_vao->create();
	m_vao->bind();

	auto m_vertexBuffer = new QOpenGLBuffer;
	m_vertexBuffer->setUsagePattern(QOpenGLBuffer::StaticDraw);
	m_vertexBuffer->create();
	m_vertexBuffer->bind();
	float vertices[] = {-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
			    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  0.0f, 0.0f,
			    1.0f,  -1.0f, 1.0f, 1.0f, 1.0f,  1.0f,  1.0f, 0.0f};

	m_vertexBuffer->allocate(sizeof(vertices));
	m_vertexBuffer->write(0, vertices, sizeof(vertices));
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
			      (void *)0);
	glEnableVertexAttribArray(0);
	m_vertexBuffer->release();
	m_vao->release();
}

void STThread::initTexture()
{
	m_strawberryTexture =
		new QOpenGLTexture(QImage(":/mark/image/main/strawberry2.png"));
	m_bombTexture =
		new QOpenGLTexture(QImage(":/mark/image/main/bomb2.png"));
	//m_strawberryTexture = new QOpenGLTexture(
	//	QImage("C:/Users/luweijia.YUPAOPAO/Desktop/strawberry2.png"));
	//m_bombTexture = new QOpenGLTexture(
	//	QImage("C:/Users/luweijia.YUPAOPAO/Desktop/bomb2.png"));
}
