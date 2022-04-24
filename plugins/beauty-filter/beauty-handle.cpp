#include "beauty-handle.h"
#include <QMatrix4x4>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QtMath>
#include <QJsonDocument>
#include <QApplication>
#include <QEvent>

#define STRAWBERRY_TIME 4

BeautyHandle::BeautyHandle(obs_source_t *context) : m_source(context)
{
	initOpenGL();

	/*QTimer* timer = new QTimer;
	timer->setInterval(10 * 1000);
	QObject* obj = new QObject;
	QObject::connect(timer, &QTimer::timeout, obj, [=]() {
		m_gameStickerType = GameStickerType::Bomb;
		m_curRegion = 2;
		m_gameStartTime = QDateTime::currentMSecsSinceEpoch();
	});
	timer->start();*/
}

 BeautyHandle::~BeautyHandle()
 {
	 clearGLResource();
 }

void BeautyHandle::initOpenGL()
{
	m_glctx.initGLContext();
	m_glctx.makeCurrentContext();
	initShader();
	initStrawberryShader();
	initVertexData();
	initStrawVertexData();

	glGenFramebuffers(1, &m_fbo);

	effectHandlerInited = false;
	m_effectHandle = new EffectHandle;
	auto ret = m_effectHandle->initializeHandle();
	effectHandlerInited = BEF_RESULT_SUC == ret;

	m_dectector = new BEDectector;
	m_dectector->initializeDetector(false);

	m_glctx.doneCurrentContext();
}

void BeautyHandle::checkBeautySettings()
{
	QString filterPath;
	QString stickerPath;
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
					m_effectHandle->updateComposerNode(id, isAdd ? 1 : 0, value);
				}
			} else if (type == 2) { //设置滤镜
				for (int i = 0; i < arr.size(); i++) {
					auto beautyOne = arr.at(i).toObject();
					int subType = beautyOne["subType"].toInt();
					if (subType == 0) {
						m_effectHandle->setIntensity(BEF_INTENSITY_TYPE_GLOBAL_FILTER_V2, beautyOne["value"].toInt() / 100.0);
					} else if (subType == 1) {
						filterPath = beautyOne["path"].toString();
						m_effectHandle->setFilter(filterPath.toStdString());
					}
				}
			} else if (type == 3) { //设置贴纸
				for (int i = 0; i < arr.size(); i++) {
					auto beautyOne = arr.at(i).toObject();
					stickerPath = beautyOne["path"].toString();
					m_effectHandle->setStickerNoAutoPath(stickerPath.toStdString());
				}
			}
		}
		m_beautifySettings.clear();

		beautyEnabled = m_effectHandle->getComposerNodeCount() > 0 || !filterPath.isEmpty() || !stickerPath.isEmpty();
	}
	m_beautifySettingMutex.unlock();
}

void BeautyHandle::calcPosition(int &width, int &height, int w, int h)
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

void BeautyHandle::updateStrawberryData(float width, float height,
					bef_ai_face_info faceInfo, bool fliph,
					bool flip)
{
	QMutexLocker lock(&m_strawberryMutex);

	QSize strawberrySize = m_gameStickerType == Strawberry ? m_strawberry.size() : m_bomb.size();

	int s, h;
	calcPosition(s, h, width, height);
	qreal deltaTime = (QDateTime::currentMSecsSinceEpoch() - m_gameStartTime) / 1000.;
	qreal gvalue = 8 * h / (STRAWBERRY_TIME * STRAWBERRY_TIME);
	int s1 = s / qSqrt(8 * h / gvalue) * deltaTime;
	int h1 = qSqrt(2 * gvalue * h) * deltaTime -
		 0.5 * gvalue * deltaTime * deltaTime;
	QPoint center = QPoint(s1 + width / 2 - strawberrySize.width() / 2,
		strawberrySize.height() + h1);
	QRect strawberryRect = QRect(center.x(), height - center.y(),
		strawberrySize.width(),
		strawberrySize.height());
	float maskX = 2. * center.x() / width - 1;
	float maskY = -(2. * center.y() / height - 1);

	bool hit = false;
	if (faceInfo.face_count >= 1) {
		if (m_gameStickerType == Strawberry) {
			// 吃到草莓
			bef_ai_face_106 face = faceInfo.base_infos[0];
			QRect r(QPoint(fliph ? width - face.points_array[84].x
					     : face.points_array[84].x,
				       flip ? height - face.points_array[87].y
					    : face.points_array[87].y),
				QPoint(fliph ? width - face.points_array[90].x
					     : face.points_array[90].x,
				       flip ? height - face.points_array[93].y
					    : face.points_array[93].y));
			hit = strawberryRect.intersects(r);
		} else if (m_gameStickerType == Bomb) {
			bef_ai_face_106 face = faceInfo.base_infos[0];
			QRect r(QPoint(fliph ? width - face.rect.left
					     : face.rect.left,
				       flip ? height - face.rect.top
					    : face.rect.top),
				QPoint(fliph ? width - face.rect.right
					     : face.rect.right,
				       flip ? height - face.rect.bottom
					    : face.rect.bottom));
			hit = strawberryRect.intersects(r);
		}
	}

	// 草莓离开可视区域
	QRect w(0, 0, width, height);
	if (hit || !w.intersects(strawberryRect)) {
		m_gameStickerType = GameStickerType::None;
		qApp->postEvent(qApp, new QEvent((QEvent::Type)(
					      hit ? QEvent::User + 1024
						  : QEvent::User + 1025)));
	}

	float maskWith = maskX + strawberrySize.width() / width * 2;
	float maskHeight = maskY + strawberrySize.height() / height * 2;
	float vertices[] = {
		// positions         // texture coords
		maskWith, maskY,      1.0f, 0.0f, // top right
		maskWith, maskHeight, 1.0f, 1.0f, // bottom right
		maskX,    maskHeight, 0.0f, 1.0f, // bottom left

		maskWith, maskY,      1.0f, 0.0f, // top right
		maskX,    maskHeight, 0.0f, 1.0f, // bottom left
		maskX,    maskY,      0.0f, 0.0f  // top left
	};

	glBindVertexArray(m_strawberryVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_strawberryVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		     GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
			      (void *)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

obs_source_frame *BeautyHandle::processFrame(obs_source_frame *frame)
{
	static bool needDrop = false;
	checkBeautySettings();
	bool doBeauty = effectHandlerInited && beautyEnabled;
	//doBeauty = true;
	auto settings = obs_source_get_settings(m_source);
	obs_data_set_int(settings, "need_beauty", doBeauty ? 1 : 0);
	obs_data_release(settings);
	if (!doBeauty || frame->format != VIDEO_FORMAT_RGBA) {
		needDrop = true;
		return frame;
	}
	bef_ai_face_info faceInfo;
	m_dectector->setWidthAndHeight(frame->width, frame->height, frame->width * 4);
	m_dectector->detectFace(&faceInfo, frame->data[0], BEF_AI_PIX_FMT_RGBA8888,
				frame->flip ? BEF_AI_CLOCKWISE_ROTATE_180 : BEF_AI_CLOCKWISE_ROTATE_0,
				BEF_DETECT_MODE_VIDEO | BEF_FACE_DETECT, false);

	m_glctx.makeCurrentContext();

	bool frameInfoChanged = (m_lastWidth != frame->width || m_lastHeight != frame->height);
	m_lastWidth = frame->width;
	m_lastHeight = frame->height;

	if (frameInfoChanged) {
		deletePBO();
		createPBO(frame->width, frame->height);

		deleteTextures();
		createTextures(frame->width, frame->height);
	}

	glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data[0]);
	glBindTexture(GL_TEXTURE_2D, 0);

	bool copyDraw = frame->flip_h || frame->flip;
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

			if (frame->flip_h) {
				model.scale(-1, 1);
			}

			if (frame->flip) {
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
	auto result = m_effectHandle->process(copyDraw ? m_outputTexture2 : m_backgroundTexture, m_outputTexture, frame->width, frame->height, false, timestamp);	

	//草莓
	if (m_gameStickerType == GameStickerType::Strawberry || m_gameStickerType == GameStickerType::Bomb)
	{
		bool strawberry = m_gameStickerType == Strawberry;
		updateStrawberryData(frame->width, frame->height, faceInfo,
				     frame->flip_h, frame->flip);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D, m_outputTexture, 0);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			printf("GLError BERender::strawberry \n");
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, strawberry ? m_strawberryTexture : m_bombTexture);
		
		//glViewport(0, 0, frame->width, frame->height);
		glUseProgram(m_strawberryShader);
		glBindVertexArray(m_strawberryVao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glUseProgram(0);
		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_BLEND);
	}

	//{
	//	bef_ai_face_106 face = faceInfo.base_infos[0];
	//	int hscal = frame->flip_h ? -1 : 1;
	//	int vscal = frame->flip ? -1 : 1;
	//	float vertices[] = {
	//		// positions         // texture coords
	//		(face.points_array[90].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[87].y * 2 / frame->height - 1) *
	//			vscal,
	//		1.0f,
	//		0.0f, // top right
	//		(face.points_array[90].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[93].y * 2 / frame->height - 1) *
	//			vscal,
	//		1.0f,
	//		1.0f, // bottom right
	//		(face.points_array[84].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[93].y * 2 / frame->height - 1) *
	//			vscal,
	//		0.0f,
	//		1.0f, // bottom left
	//		(face.points_array[90].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[87].y * 2 / frame->height - 1) *
	//			vscal,
	//		1.0f,
	//		0.0f, // top right
	//		(face.points_array[84].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[93].y * 2 / frame->height - 1) *
	//			vscal,
	//		0.0f,
	//		1.0f, // bottom left
	//		(face.points_array[84].x * 2 / frame->width - 1) *
	//			hscal,
	//		(face.points_array[87].y * 2 / frame->height - 1) *
	//			vscal,
	//		0.0f,
	//		0.0f // top left
	//	};

	//	glBindVertexArray(m_strawberryVao);
	//	glBindBuffer(GL_ARRAY_BUFFER, m_strawberryVbo);
	//	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
	//		     GL_STATIC_DRAW);
	//	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
	//			      4 * sizeof(float), (void *)0);
	//	glEnableVertexAttribArray(0);

	//	glBindBuffer(GL_ARRAY_BUFFER, 0);
	//	glBindVertexArray(0);

	//	glEnable(GL_BLEND);
	//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	//			       GL_TEXTURE_2D, m_outputTexture, 0);
	//	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	//	if (status != GL_FRAMEBUFFER_COMPLETE) {
	//		printf("GLError BERender::strawberry \n");
	//	}

	//	glActiveTexture(GL_TEXTURE0);
	//	glBindTexture(GL_TEXTURE_2D, m_gameStickerType == Strawberry
	//					     ? m_strawberryTexture
	//					     : m_bombTexture);

	//	//glViewport(0, 0, frame->width, frame->height);
	//	glUseProgram(m_strawberryShader);
	//	glBindVertexArray(m_strawberryVao);
	//	glDrawArrays(GL_TRIANGLES, 0, 6);
	//	glUseProgram(0);
	//	glBindVertexArray(0);
	//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//	glDisable(GL_BLEND);
	//}

	if (m_glctx.usePbo()) {
		PBOReader::DisposableBuffer buf;
		if (m_reader->read(m_outputTexture, frame->width, frame->height, buf)) {
			if (needDrop)
				needDrop = false;
			else {
				memcpy(frame->data[0], buf.get(), frame->width * frame->height * 4);
				frame->flip = false;
				frame->flip_h = false;
			}
		}
	}

	m_glctx.doneCurrentContext();
	return frame;
}

void BeautyHandle::updateBeautySettings(obs_data_t *beautySetting)
{
	const char *s = obs_data_get_string(beautySetting, "beautifySetting");
	QMutexLocker locker(&m_beautifySettingMutex);
	m_beautifySettings.append(s);
}

void BeautyHandle::updateStrawberrySettings(obs_data_t *setting)
{
	const char *s = obs_data_get_string(setting, "face_sticker_info");
	QJsonObject obj = QJsonDocument::fromJson(s).object();
	if (obj.isEmpty())
		return;

	QMutexLocker lock(&m_strawberryMutex);
	m_gameStickerType = (GameStickerType)obj["gameType"].toInt();
	if (m_gameStickerType == Bomb || m_gameStickerType == Strawberry) {
		m_curRegion = obj["region"].toInt();
		m_gameStartTime = QDateTime::currentMSecsSinceEpoch();
	}
}

void BeautyHandle::initShader()
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

void BeautyHandle::initStrawberryShader()
{
	auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
	auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char *vstr = R"(
			attribute vec4 vertex;
			varying vec2 v_texCoord;

			void main()
			{
				v_texCoord = vertex.zw;
				gl_Position = vec4(vertex.xy, 0.0, 1.0);
			}
			)";

	char *fstr = R"(
			precision mediump float;
			varying vec2 v_texCoord;

			uniform sampler2D strawberry;
			void main()
			{
				gl_FragColor = texture2D(strawberry, v_texCoord);
			}
			)";
	/*char *fstr = R"(
			precision mediump float;
			varying vec2 v_texCoord;

			uniform sampler2D strawberry;
			void main()
			{
				gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
			}
			)";*/

	glShaderSource(vertexShader, 1, &vstr, NULL);
	glCompileShader(vertexShader);

	GLint success;
	GLchar infoLog[1024];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
	}

	glShaderSource(fragmentShader, 1, &fstr, NULL);
	glCompileShader(fragmentShader);

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
	}

	m_strawberryShader = glCreateProgram();
	glAttachShader(m_strawberryShader, vertexShader);
	glAttachShader(m_strawberryShader, fragmentShader);
	glLinkProgram(m_strawberryShader);

	glGetProgramiv(m_strawberryShader, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(m_strawberryShader, 1024, NULL, infoLog);
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glUseProgram(m_strawberryShader);
	glUniform1i(glGetUniformLocation(m_strawberryShader, "strawberry"), 0);
	glUseProgram(0);
}

void BeautyHandle::initVertexData()
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

void BeautyHandle::initStrawVertexData()
{
	glGenVertexArrays(1, &m_strawberryVao);
	glGenBuffers(1, &m_strawberryVbo);
}

void BeautyHandle::createTextures(int w, int h)
{
	glGenTextures(1, &m_backgroundTexture);
	glBindTexture(GL_TEXTURE_2D, m_backgroundTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenTextures(1, &m_outputTexture);
	glBindTexture(GL_TEXTURE_2D, m_outputTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, NULL);
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

	//QImage strawberry = QImage("D:\\xxq-recon\\yuerlive\\resource\\image\\main\\strawberry2.png");
	m_strawberry.load(":/mark/image/main/strawberry2.png");
	m_strawberry.convertTo(QImage::Format_RGBA8888);
	
	glGenTextures(1, &m_strawberryTexture);
	glBindTexture(GL_TEXTURE_2D, m_strawberryTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_strawberry.width(),
		m_strawberry.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, m_strawberry.bits());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	m_bomb.load(":/mark/image/main/bomb2.png");
	m_bomb.convertTo(QImage::Format_RGBA8888);

	glGenTextures(1, &m_bombTexture);
	glBindTexture(GL_TEXTURE_2D, m_bombTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_bomb.width(),
		m_bomb.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, m_bomb.bits());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void BeautyHandle::deleteTextures()
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

	if (m_strawberryTexture) {
		glDeleteTextures(1, &m_strawberryTexture);
	}

	if (m_bombTexture)
	{
		glDeleteTextures(1, &m_bombTexture);
	}
}

void BeautyHandle::freeResource()
{
	m_effectHandle->releaseHandle();
	delete m_effectHandle;
	m_effectHandle = nullptr;

	if (m_dectector)
	{
		m_dectector->releaseDetector();
		delete m_dectector;
		m_dectector = nullptr;
	}
}

void BeautyHandle::clearGLResource()
{
	m_glctx.makeCurrentContext();

	deleteTextures();
	freeResource();

	glDeleteProgram(m_shader);
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteProgram(m_strawberryShader);
	glDeleteVertexArrays(1, &m_strawberryVao);
	glDeleteBuffers(1, &m_strawberryVbo);

	deletePBO();

	m_glctx.doneCurrentContext();
}

void BeautyHandle::createPBO(int w, int h)
{
	m_reader = new PBOReader(w * h * 4);
}

void BeautyHandle::deletePBO()
{
	if (m_reader) {
		m_reader->release();
		delete m_reader;
		m_reader = nullptr;
	}
}
