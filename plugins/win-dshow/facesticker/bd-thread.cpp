#include "bd-thread.h"
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
#include <QJsonArray>
#include <QDebug>
#include "..\win-dshow.h"
#include "BEFEffectGLContext.h"
#include "PBOReader.h"

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

BDThread::BDThread(DShowInput *dsInput) : m_dshowInput(dsInput)
	, m_frameQueue(1)
	, m_cacheFrame(av_frame_alloc())
{
	moveToThread(this);
}

BDThread::~BDThread()
{
	if (m_dataBuffer)
		av_free(m_dataBuffer);

	av_frame_free(&m_cacheFrame);
}

void BDThread::waitStarted()
{
	start(QThread::TimeCriticalPriority);
	waitMutex.lock();
	waitCondition.wait(&waitMutex);
	waitMutex.unlock();
}

void BDThread::run()
{
	QString filterPath;
	QString stickerPath;

	BEF::BEFEffectGLContext ctx;
	ctx.initGLContext();
	ctx.makeCurrentContext();
	initShader();
	initVertexData();

	glGenFramebuffers(1, &m_fbo);

	m_running = true;

	effectHandlerInited = false;
	m_stFunc = new EffectHandle;
	auto ret = m_stFunc->initializeHandle();
	effectHandlerInited = BEF_RESULT_SUC == ret;

	qDebug() << "SenseTime init result: " << effectHandlerInited;
	if (!effectHandlerInited) {
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
		if (!m_beautifySettings.isEmpty())
		{
			for (auto iter = m_beautifySettings.begin(); iter != m_beautifySettings.end(); iter++)
			{
				//设置项总共有美颜 美妆 滤镜 贴纸
				//美颜设置参数=> id value
				//美妆设置参数=> id value
				//贴纸=>传个文件夹的名字
				//滤镜=>传滤镜文件夹的名字，切换滤镜的值需要有一个id，id有一个对应关系
				QJsonDocument jd = QJsonDocument::fromJson((*iter).toUtf8());
				auto p = jd.object();
				int type = p["type"].toInt();
				auto arr = p["beautyParam"].toArray();
				if (type == 1) { //设置美颜
					for (int i = 0; i < arr.size(); i++) {
						auto beautyOne = arr.at(i).toObject();
						int id = beautyOne["id"].toInt();
						float value = beautyOne["value"].toInt() / 100.0;
						bool isAdd = beautyOne["isAdd"].toBool();
						m_stFunc->updateComposerNode(id, isAdd ? 1 : 0, value);
					}
				} else if (type == 2) { //设置滤镜
					for (int i = 0; i < arr.size(); i++) {
						auto beautyOne = arr.at(i).toObject();
						int subType = beautyOne["subType"].toInt();
						if (subType == 0) {
							m_stFunc->setIntensity(BEF_INTENSITY_TYPE_GLOBAL_FILTER_V2, beautyOne["value"].toInt() / 100.0);
						} else if (subType == 1) {
							filterPath = beautyOne["path"].toString();
							m_stFunc->setFilter(filterPath.toStdString());
						}
					}
				} else if (type == 3) { //设置贴纸
					for (int i = 0; i < arr.size(); i++) {
						auto beautyOne = arr.at(i).toObject();
						stickerPath = beautyOne["path"].toString();
						m_stFunc->setStickerNoAutoPath(stickerPath.toStdString());
					}
				}
			}
			m_beautifySettings.clear();

			m_needBeautify = m_stFunc->getComposerNodeCount() > 0 || !filterPath.isEmpty() || !stickerPath.isEmpty();
		}
		m_beautifySettingMutex.unlock();

		processImage(frame.avFrame, frame.startTime, &ctx);
	}

Clear:
	deleteTextures();

	freeResource();

	glDeleteProgram(m_shader);
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteFramebuffers(1, &m_fbo);

	ctx.doneCurrentContext();
	qDebug() << "STThread stopped...";
}

void BDThread::updateInfo(const char *data)
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

void BDThread::updateGameInfo(GameStickerType type, int region)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	m_gameStickerType = type;
	if (type == Bomb || type == Strawberry) {
		m_curRegion = region;
		m_gameStartTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void BDThread::updateBeautifySetting(QString setting)
{
	QMutexLocker locker(&m_beautifySettingMutex);
	m_beautifySettings.append(setting);
}

void BDThread::ensureCacheBuffer(size_t size)
{
	if (!m_dataBuffer) {
		m_dataBuffer = (unsigned char *)av_malloc(size);
	} else if (m_dataBufferSize < size) {
		m_dataBuffer = (unsigned char *)av_realloc(m_dataBuffer, size);
	}

	m_dataBufferSize = size;
}

void BDThread::addFrame(unsigned char *data, size_t size, long long startTime, int w, int h, int f)
{
	if (!m_running)
		return;

	FrameInfo info;
	info.startTime = startTime;

	uint8_t *td[AV_NUM_DATA_POINTERS] = { 0 };
	int tl[AV_NUM_DATA_POINTERS] = { 0 };
	av_image_fill_arrays(td, tl, data, (AVPixelFormat)f, w, h, 1);

	auto as = av_image_get_buffer_size((AVPixelFormat)f, w, h, 16);
	ensureCacheBuffer(as);

	av_image_copy_to_buffer(m_dataBuffer, as, td, tl, (AVPixelFormat)f, w, h, 16);
	av_image_fill_arrays(m_cacheFrame->data, m_cacheFrame->linesize, m_dataBuffer, (AVPixelFormat)f, w, h, 16);

	m_cacheFrame->width = w;
	m_cacheFrame->height = h;
	m_cacheFrame->format = f;
	m_cacheFrame->pts = startTime;
	info.avFrame = m_cacheFrame;
	info.startTime = startTime;
	m_frameQueue.try_enqueue(info);
}

void BDThread::addFrame(AVFrame *frame, long long startTime)
{
	if (!m_running)
		return;

	FrameInfo info;

	auto size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width, frame->height, 16);
	ensureCacheBuffer(size);

	av_image_copy_to_buffer(m_dataBuffer, size, frame->data, frame->linesize, (AVPixelFormat)frame->format, frame->width, frame->height, 16);
	av_image_fill_arrays(m_cacheFrame->data, m_cacheFrame->linesize, m_dataBuffer, (AVPixelFormat)frame->format, frame->width, frame->height, 16);
	m_cacheFrame->width = frame->width;
	m_cacheFrame->height = frame->height;
	m_cacheFrame->format = frame->format;
	info.avFrame = m_cacheFrame;
	info.startTime = startTime;
	m_frameQueue.try_enqueue(info);
}
#include <QMatrix4x4>
void BDThread::processImage(AVFrame *frame, quint64 ts, BEF::BEFEffectGLContext *ctx)
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

	if (frameInfoChanged) {
		if (m_outDataBuffer) 
			av_freep(&m_outDataBuffer);
		
		m_outDataBuffer = (uint8_t *)av_mallocz(av_image_get_buffer_size(AV_PIX_FMT_RGBA, frame->width + 16, frame->height, 16));
		if (m_swsctx) {
			sws_freeContext(m_swsctx);
			m_swsctx = NULL;
		}

		flip = AV_PIX_FMT_BGRA == frame->format || AV_PIX_FMT_BGR0 == frame->format;
		m_swsctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height, AVPixelFormat::AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);

		deletePBO();
		createPBO(frame->width, frame->height);

		deleteTextures();
		createTextures(frame->width, frame->height);
	}

	uint8_t *out[8] = {m_outDataBuffer};
	int ls[8] = { frame->width * 4 };
	int ret = sws_scale(m_swsctx, (const uint8_t *const *)(frame->data), frame->linesize, 0, frame->height, out, ls);

	glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_outDataBuffer);
	glBindTexture(GL_TEXTURE_2D, 0);

	bool copyDraw = m_dshowInput->flipH || flip;
	if (copyDraw) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D, m_outputTexture2, 0);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("GLError BERender::drawFace \n");
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);

		glViewport(0, 0, frame->width, frame->height);
		glClearColor(0, 0, 0, 0);
		glBindVertexArray(m_vao);
		glEnableVertexAttribArray(0);
		glUseProgram(m_shader);
		{
			QMatrix4x4 model;
			model.scale(1, -1);

			if (m_dshowInput->flipH) {
				model.scale(-1, 1);
			}

			if (flip) {
				model.scale(1, -1);
			}
			glUniformMatrix4fv(glGetUniformLocation(m_shader,
								"model"),
					   1, GL_FALSE, model.constData());
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		glBindVertexArray(0);
		glUseProgram(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	auto starTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        double timestamp = (double)starTime.count() / 1000.0;
	auto result = m_stFunc->process(copyDraw ? m_outputTexture2 : m_backgroundTexture, m_outputTexture, frame->width, frame->height, false, timestamp);	
	
	if (ctx->usePbo()) {
		PBOReader::DisposableBuffer buf;
		if (m_reader->read(m_outputTexture, frame->width, frame->height, buf)) {
			if (needDrop)
				needDrop = false;
			else
				m_dshowInput->OutputFrame((unsigned char *)buf.get(), m_textureBufferSize, ts, 0, frame->width, frame->height);
		}
	}
}

void BDThread::deleteTextures()
{
	if (m_backgroundTexture) {
		glDeleteTextures(1, &m_backgroundTexture);
	}
	if (m_outputTexture) {
		glDeleteTextures(1, &m_outputTexture);
	}

	if (m_outputTexture2) {
		glDeleteTextures(1, &m_outputTexture2);
	}
}

void BDThread::createTextures(int w, int h)
{
	glGenTextures(1, &m_backgroundTexture);
	glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenTextures(1, &m_outputTexture);
	glBindTexture(GL_TEXTURE_2D, m_outputTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h,
		     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	glGenTextures(1, &m_outputTexture2);
	glBindTexture(GL_TEXTURE_2D, m_outputTexture2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void BDThread::updateSticker(const QString &stickerId, bool isAdd)
{
	/*QMutexLocker locker(&m_stickerSetterMutex);
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
	}*/
}

void BDThread::quitThread()
{
	m_running = false;
	m_frameQueue.enqueue(FrameInfo());

	wait();

	FrameInfo info;
	while (m_frameQueue.try_dequeue(info)) {
		
	}
}

void BDThread::freeResource()
{
	if (m_swsctx)
		sws_freeContext(m_swsctx);

	if (m_outDataBuffer)
		av_freep(&m_outDataBuffer);

	m_stFunc->releaseHandle();
	delete m_stFunc;
	m_stFunc = nullptr;
}

void BDThread::setBeautifyEnabled(bool enabled)
{
	QMutexLocker locker(&m_stickerSetterMutex);
	m_needBeautify = enabled;
}

void BDThread::initShader()
{
	auto m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	auto m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char *vstr = R"(
			attribute vec4 vertex;

			varying vec2 v_texCoord;
			uniform mat4 model;

			void main()
			{
				v_texCoord = vertex.zw;
				gl_Position = model * vec4(vertex.xy, 0.0, 1.0);
			}
			)";


	char *fstr = R"(
			precision mediump float;
			varying vec2 v_texCoord;

			uniform sampler2D image;
			void main()
			{
				gl_FragColor = vec4(texture2D(image, v_texCoord).rgb, 1);
			}
			)";
	
	glShaderSource(m_vertexShader, 1, &vstr, NULL);
	glCompileShader(m_vertexShader);

	GLint success;
	GLchar infoLog[1024];
	glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(m_vertexShader, 1024, NULL, infoLog);
	}

	glShaderSource(m_fragmentShader, 1, &fstr, NULL);
	glCompileShader(m_fragmentShader);

	glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(m_fragmentShader, 1024, NULL, infoLog);
	}


	m_shader = glCreateProgram();
	glAttachShader(m_shader, m_vertexShader);
	glAttachShader(m_shader, m_fragmentShader);
	glLinkProgram(m_shader);

	glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
        if(!success)
        {
		glGetProgramInfoLog(m_shader, 1024, NULL, infoLog);
        }

	glDeleteShader(m_vertexShader);
	glDeleteShader(m_fragmentShader);

	glUseProgram(m_shader);
	glUniform1i(glGetUniformLocation(m_shader, "image"), 0);
	glUseProgram(0);
}

void BDThread::initVertexData()
{
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	float vertices[] = {-1.0f, 1.0f,  0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
			    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  0.0f, 0.0f,
			    1.0f,  -1.0f, 1.0f, 1.0f, 1.0f,  1.0f,  1.0f, 0.0f};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
			      (void *)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void BDThread::createPBO(int w, int h)
{
	m_reader = new PBOReader(w * h * 4);
}

void BDThread::deletePBO()
{
	if (m_reader) {
		m_reader->release();
		delete m_reader;
	}
}
