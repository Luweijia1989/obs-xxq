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
		return AV_PIX_FMT_BGR0;
	case VIDEO_FORMAT_Y800:
		return AV_PIX_FMT_GRAY8;
	}

	return AV_PIX_FMT_NONE;
}

STThread::STThread(DShowInput *dsInput) : m_dshowInput(dsInput)
	, m_frameQueue(1)
	, m_cacheFrame(av_frame_alloc())
{
	surface = new QOffscreenSurface(nullptr, this);
	surface->create();
	m_stFunc = new STFunction;
	moveToThread(this);
}

STThread::~STThread()
{
	if (m_dataBuffer)
		av_free(m_dataBuffer);

	av_frame_free(&m_cacheFrame);
}

void STThread::waitStarted()
{
	start(QThread::TimeCriticalPriority);
	waitMutex.lock();
	waitCondition.wait(&waitMutex);
	waitMutex.unlock();
}

void STThread::run()
{
	initOpenGLContext();
	initShader();
	initVertexData();
	initTexture();

	ctx->makeCurrent(surface);

	m_running = true;
	if (!g_st_checkpass) {
		m_running = false;
		waitCondition.wakeOne();
		goto Clear;
	}


	m_stFunc->initSenseTimeEnv();
	qDebug() << "SenseTime init result: " << m_stFunc->stInited();
	if (!m_stFunc->stInited()) {
		m_running = false;
		waitCondition.wakeOne();
		goto Clear;
	}
	waitCondition.wakeOne();
	while (m_running)
	{
		FrameInfo frame;
		m_frameQueue.wait_dequeue(frame);

		if (!m_running)
			break;

		m_beautifySettingMutex.lock();
		for (auto iter = m_beautifySettings.begin(); iter != m_beautifySettings.end(); iter++)
		{
			QJsonDocument jd = QJsonDocument::fromJson((*iter).toUtf8());
			m_stFunc->updateBeautifyParam(jd.object());
		}
		m_beautifySettings.clear();
		m_beautifySettingMutex.unlock();

		processImage(frame.avFrame, frame.startTime);
	}

Clear:
	if (m_fbo)
		delete m_fbo;

	deleteTextures();
	m_strawberryTexture->destroy();
	delete m_strawberryTexture;
	m_bombTexture->destroy();
	delete m_bombTexture;

	deletePBO();

	freeResource();

	delete m_vertexShader;
	delete m_vertexShader2;
	delete m_fragmentShader;
	delete m_fragmentShader2;
	delete m_shader;
	delete m_flipShader;
	delete m_vao;

	ctx->doneCurrent();
	delete ctx;
	qDebug() << "STThread stopped...";
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

void STThread::updateBeautifySetting(QString setting)
{
	QMutexLocker locker(&m_beautifySettingMutex);
	m_beautifySettings.append(setting);
}

void STThread::ensureCacheBuffer(size_t size)
{
	if (!m_dataBuffer) {
		m_dataBuffer = (unsigned char *)av_malloc(size);
	} else if (m_dataBufferSize < size) {
		m_dataBuffer = (unsigned char *)av_realloc(m_dataBuffer, size);
	}

	m_dataBufferSize = size;
}

void STThread::addFrame(unsigned char *data, size_t size, long long startTime, int w, int h, int f)
{
	if (!m_running)
		return;

	FrameInfo info;
	info.startTime = startTime;

	uint8_t *td[AV_NUM_DATA_POINTERS] = { 0 };
	int tl[AV_NUM_DATA_POINTERS] = { 0 };
	av_image_fill_arrays(td, tl, data, (AVPixelFormat)f, w, h, 1);

	auto as = av_image_get_buffer_size((AVPixelFormat)f, w, h, 32);
	ensureCacheBuffer(as);

	av_image_copy_to_buffer(m_dataBuffer, as, td, tl, (AVPixelFormat)f, w, h, 32);
	av_image_fill_arrays(m_cacheFrame->data, m_cacheFrame->linesize, m_dataBuffer, (AVPixelFormat)f, w, h, 32);

	m_cacheFrame->width = w;
	m_cacheFrame->height = h;
	m_cacheFrame->format = f;
	m_cacheFrame->pts = startTime;
	info.avFrame = m_cacheFrame;
	info.startTime = startTime;
	m_frameQueue.enqueue(info);
}

void STThread::addFrame(AVFrame *frame, long long startTime)
{
	if (!m_running)
		return;

	FrameInfo info;

	auto size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width, frame->height, 32);
	ensureCacheBuffer(size);

	av_image_copy_to_buffer(m_dataBuffer, size, frame->data, frame->linesize, (AVPixelFormat)frame->format, frame->width, frame->height, 32);
	av_image_fill_arrays(m_cacheFrame->data, m_cacheFrame->linesize, m_dataBuffer, (AVPixelFormat)frame->format, frame->width, frame->height, 32);
	m_cacheFrame->width = frame->width;
	m_cacheFrame->height = frame->height;
	m_cacheFrame->format = frame->format;
	info.avFrame = m_cacheFrame;
	info.startTime = startTime;
	m_frameQueue.enqueue(info);
}

void STThread::copyTexture(QOpenGLTexture *texture)
{
	m_fbo->bind();
	glClearColor(0, 0, 0, 0);

	glActiveTexture(GL_TEXTURE0);
	texture->bind();
	glViewport(0, 0, texture->width(), texture->height());

	m_vao->bind();
	m_flipShader->bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	m_vao->release();
	m_flipShader->release();
	m_fbo->release();
}

void STThread::processImage(AVFrame *frame, quint64 ts)
{
	static bool needDrop = false;
	QMutexLocker locker(&m_stickerSetterMutex);
	if (m_stickers.isEmpty() && m_gameStickerType == None && !m_needBeautify) {
		m_dshowInput->OutputFrame(frame, ts, m_dshowInput->flipH);
		needDrop = true;
		return;
	}

	if (m_dshowInput->videoConfig.cx != frame->width || m_dshowInput->videoConfig.cy != frame->height)
		return;

	quint64 m_textureBufferSize = frame->width * frame->height * 4;
	bool frameInfoChanged = (m_lastWidth != frame->width || m_lastHeight != frame->height || m_lastFormat != frame->format);
	m_lastWidth = frame->width;
	m_lastHeight = frame->height;
	m_lastFormat = frame->format;

	bool needMask = m_gameStickerType != None;
	bool needSticker = !m_stickers.isEmpty();
	
	if (frameInfoChanged) {
		if (m_swsRetFrame) {
			av_freep(m_swsRetFrame->data);
			av_frame_free(&m_swsRetFrame);
		}
		m_swsRetFrame = av_frame_alloc();
		av_image_alloc(m_swsRetFrame->data, m_swsRetFrame->linesize, frame->width, frame->height, AV_PIX_FMT_RGBA, 32);
		if (m_swsctx) {
			sws_freeContext(m_swsctx);
			m_swsctx = NULL;
		}

		flip = AV_PIX_FMT_BGRA == frame->format || AV_PIX_FMT_BGR0 == frame->format;
		m_swsctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);

		if (m_fbo)
			delete m_fbo;

		m_fbo = new QOpenGLFramebufferObject(frame->width, frame->height);

		deleteTextures();

		m_backgroundTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
		m_backgroundTexture->setSize(frame->width, frame->height);
		m_backgroundTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
		m_backgroundTexture->allocateStorage();

		createTextures(frame->width, frame->height);

		deletePBO();
		createPBO(m_textureBufferSize);
	}

	int ret = sws_scale(m_swsctx, (const uint8_t *const *)(frame->data), frame->linesize, 0, frame->height, m_swsRetFrame->data, m_swsRetFrame->linesize);

	m_backgroundTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, m_swsRetFrame->data[0]);

	m_stFunc->doFaceDetect(m_needBeautify, needSticker, m_swsRetFrame->data[0], frame->width, frame->height, flip);

	QOpenGLTexture *nextSrc = m_backgroundTexture;
	if (m_needBeautify) {//是否美颜
		nextSrc = m_stFunc->doBeautify(m_backgroundTexture, m_beautify, m_makeup, m_filter, frame->width, frame->height, m_dshowInput->flipH, flip);
	}

	m_stFunc->flipFaceDetect(flip, m_dshowInput->flipH, frame->width, frame->height); // 翻转得到实际的人脸关键点信息

	float maskX = 0.;
	float maskY = 0.;
	float maskWidth = (float)m_strawberryTexture->width() / frame->width * 2;
	float maskHeight = -(float)m_strawberryTexture->height() / frame->height * 2;
	if (needMask) {
		int s, h;
		calcPosition(s, h, frame->width, frame->height);
		qreal deltaTime = (QDateTime::currentMSecsSinceEpoch() - m_gameStartTime) / 1000.;
		qreal gvalue = 8 * h / (STRAWBERRY_TIME * STRAWBERRY_TIME);
		int s1 = s / qSqrt(8 * h / gvalue) * deltaTime;
		int h1 = qSqrt(2 * gvalue * h) * deltaTime - 0.5 * gvalue * deltaTime * deltaTime;
		QPoint center = QPoint(s1 + frame->width / 2 - m_strawberryTexture->width() / 2, m_strawberryTexture->height() + h1);
		QRect strawberryRect = QRect(center.x(), frame->height - center.y(), m_strawberryTexture->width(), m_strawberryTexture->height());
		maskX = 2. * center.x() / frame->width - 1;
		maskY = 2. * center.y() / frame->height - 1;
	
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
	
		QRect w(0, 0, frame->width, frame->height);
		if (hit || !w.intersects(strawberryRect)) {
			updateGameInfo(None, -1);
			needMask = false;
			qApp->postEvent(qApp, new QEvent((QEvent::Type)(hit ? QEvent::User + 1024 : QEvent::User + 1025)));
		}
	}

	m_fbo->bind();

	glActiveTexture(GL_TEXTURE0);
	nextSrc->bind();

	glActiveTexture(GL_TEXTURE1);
	if (m_gameStickerType == Strawberry)
		m_strawberryTexture->bind();
	else if (m_gameStickerType == Bomb)
		m_bombTexture->bind();

	glViewport(0, 0, frame->width, frame->height);
	glClearColor(0, 0, 0, 0);
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

	m_vao->release();
	m_shader->release();
	m_fbo->release();

	if (needSticker) {
		m_stFunc->doFaceSticker(m_fbo->texture(), m_outputTexture->textureId(), frame->width, frame->height, m_dshowInput->flipH, flip);
		copyTexture(m_outputTexture);
	}

	m_fbo->bind();
	static int index = 0;
	int nextIndex = 0; // pbo index used for next frame
	index = (index + 1) % 2;
	nextIndex = (index + 1) % 2;

	QOpenGLExtraFunctions *extraFuncs = ctx->extraFunctions();
	extraFuncs->glReadBuffer(GL_COLOR_ATTACHMENT0);

	m_pbos[index]->bind();
	glReadPixels(0, 0, frame->width, frame->height, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	m_pbos[index]->release();
	m_pbos[nextIndex]->bind();
	auto src = m_pbos[nextIndex]->map(QOpenGLBuffer::ReadOnly);
	if (src) {
		if (needDrop)
			needDrop = false;
		else
			m_dshowInput->OutputFrame(DShow::VideoFormat::ARGB, (unsigned char *)src, m_textureBufferSize, ts, 0, frame->width, frame->height);
		m_pbos[nextIndex]->unmap();
	}
	m_pbos[nextIndex]->release();
	m_fbo->release();
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
	m_outputTexture->setSize(w, h);
	m_outputTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_outputTexture->allocateStorage();

	m_beautify = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_beautify->setSize(w, h);
	m_beautify->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_beautify->allocateStorage();

	m_makeup = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_makeup->setSize(w, h);
	m_makeup->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_makeup->allocateStorage();

	m_filter = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_filter->setSize(w, h);
	m_filter->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_filter->allocateStorage();
}

void STThread::createPBO(quint64 size)
{
	for (int i=0; i<2; i++)
	{
		QOpenGLBuffer *pbo = new QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer);
		pbo->create();
		pbo->bind();
		pbo->allocate(size);
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
	if (!m_stFunc)
		return;
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

void STThread::quitThread()
{
	m_running = false;
	m_frameQueue.enqueue(FrameInfo());

	wait();

	FrameInfo info;
	while (m_frameQueue.try_dequeue(info)) {
		
	}
}

void STThread::freeResource()
{
	if (m_swsctx)
		sws_freeContext(m_swsctx);

	if (m_swsRetFrame)
		av_frame_free(&m_swsRetFrame);

	m_stFunc->freeStResource();
	delete m_stFunc;
	m_stFunc = nullptr;
}

void STThread::setBeautifyEnabled(bool enabled)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	m_needBeautify = enabled;
}

void STThread::calcPosition(int &width, int &height, int w, int h)
{
	if (m_curRegion == -1) {
		width = 0;
		height = 0;
	}

	int totalCount = 15;

	int stepx = w / 5;
	int stepy = h / 3;
	int x_r = m_curRegion % 5;
	int y_r = m_curRegion / 5;

	width = (x_r < 2 ? (x_r - 2.5) * stepx : (x_r - 1.5) * stepx);
	height = h - (y_r + 0.5) * stepy;
}

void STThread::initShader()
{
	m_vertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
	m_vertexShader2 = new QOpenGLShader(QOpenGLShader::Vertex);
	m_fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment);
	m_fragmentShader2 = new QOpenGLShader(QOpenGLShader::Fragment);
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

	b = m_vertexShader2->compileSourceCode(R"(
                                               #version 330 core
                                               layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

                                               out vec2 TexCoords;
                                               
                                               void main()
                                               {
                                                   TexCoords = vertex.zw;
                                                   gl_Position = vec4(vertex.x, -vertex.y, 1, 1);
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

	b = m_fragmentShader2->compileSourceCode(R"(
                                            #version 330 core
                                            in vec2 TexCoords;
                                            out vec4 color;

                                            uniform sampler2D image;
                                            void main()
                                            {
                                                color = vec4(texture(image, TexCoords).rgb, 1);
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

	m_flipShader = new QOpenGLShaderProgram;
	m_flipShader->addShader(m_vertexShader2);
	m_flipShader->addShader(m_fragmentShader2);
	m_flipShader->link();

	m_flipShader->bind();
	m_flipShader->setUniformValue("image", 0);
	m_flipShader->release();
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
