#include "qt-opengl-display.hpp"
#include <QDebug>
#include <qopenglframebufferobject.h>
#include <QQuickWindow>
#include <QTimer>
#include <qopenglcontext.h>
#include <qopenglextrafunctions.h>
#include <qelapsedtimer.h>

#include "display-helpers.hpp"

static inline void startRegion(int vX, int vY, int vCX, int vCY, float oL, float oR, float oT, float oB)
{
	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(vX, vY, vCX, vCY);
	gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

static inline void endRegion()
{
	gs_viewport_pop();
	gs_projection_pop();
}

FBORenderer::FBORenderer()
{
	initializeOpenGLFunctions();

	const char *vertex_str = R"(#version 330 core
                                layout (location = 0) in vec3 aPos;
                                layout (location = 1) in vec3 aColor;
                                layout (location = 2) in vec2 aTexCoord;

                                out vec3 ourColor;
                                out vec2 TexCoord;

                                void main()
                                {
                                    gl_Position = vec4(aPos, 1.0);
                                    ourColor = aColor;
                                    TexCoord = vec2(aTexCoord.x, aTexCoord.y);
                                })";

	const char *fragment_str = R"(#version 330 core
                                out vec4 FragColor;

                                in vec3 ourColor;
                                in vec2 TexCoord;

                                // texture sampler
                                uniform sampler2D texture1;

                                void main()
                                {
                                    FragColor = texture(texture1, TexCoord);
                                })";

	if (!program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vertex_str)) {
		qDebug() << "compiler vertex error";
		exit(0);
	}
	if (!program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fragment_str)) {
		qDebug() << "compiler fragment error";
		exit(0);
	}

	program.link();
	program.bind();

	float vertices[] = {
		// positions          // colors           // texture coords
		1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
		1.0f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
		-1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f  // top left
	};
	unsigned int indices[] = {
		0, 1, 3, // first triangle
		1, 2, 3  // second triangle
	};

	vao.create();
	vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	ebo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	vbo.create();
	ebo.create();

	vao.bind();

	vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo.bind();
	vbo.allocate(vertices, sizeof(vertices));

	ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);
	ebo.bind();
	ebo.allocate(indices, sizeof(indices));

	program.setAttributeBuffer(0, GL_FLOAT, 0, 3, 8 * sizeof(float));
	program.enableAttributeArray(0);

	program.setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, 8 * sizeof(float));
	program.enableAttributeArray(1);

	program.setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 2, 8 * sizeof(float));
	program.enableAttributeArray(2);

	vao.release();
	program.release();

	glGenBuffers(1, &unpack_buffer);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
	auto size = 3840 * 2160 * 4;
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

FBORenderer::~FBORenderer()
{
	obs_display_remove_draw_callback(display, OBSRender, this);
	obs_display_destroy(display);

	if (unpack_buffer)
		glDeleteBuffers(1, &unpack_buffer);

	if (texture)
		glDeleteTextures(1, &texture);

	if (backup_texture)
		glDeleteTextures(1, &backup_texture);
}

void FBORenderer::render()
{
	{
		QMutexLocker locker(&data_mutex);
		if (size_changed) {
			size_changed = false;

			if (texture)
				backup_texture = texture;

			glGenTextures(1, &texture);

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cache_srclinesize / 4, cache_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if (texture) {
			if (!map_buffer) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
				map_buffer = QOpenGLContext::currentContext()->extraFunctions()->glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 3840 * 2160 * 4,
														  GL_MAP_WRITE_BIT);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			}

			if (map_buffer_ready) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
				QOpenGLContext::currentContext()->extraFunctions()->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

				glBindTexture(GL_TEXTURE_2D, texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cache_srclinesize / 4, cache_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				glBindTexture(GL_TEXTURE_2D, 0);

				if (backup_texture) {
					glDeleteTextures(1, &backup_texture);
					backup_texture = 0;
				}

				map_buffer_ready = false;
				map_buffer = nullptr;
			}
		}
	}

	if (!texture && !backup_texture)
		return;

	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (backup_texture)
		glBindTexture(GL_TEXTURE_2D, backup_texture);
	else if (texture)
		glBindTexture(GL_TEXTURE_2D, texture);

	program.bind();

	vao.bind();

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	vao.release();
	program.release();

	glBindTexture(GL_TEXTURE_2D, 0);
}

void FBORenderer::textureDataCallbackInternal(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height)
{
	data_mutex.lock();
	if (linesize != cache_linesize || src_height != cache_height || src_linesize != cache_srclinesize) {
		size_changed = true;
		cache_linesize = linesize;
		cache_srclinesize = src_linesize;
		cache_height = src_height;
	}

	if (map_buffer) {
		if (cache_linesize == cache_srclinesize)
			memcpy(map_buffer, data, cache_linesize * src_height);
		else {
			auto ptr = (uint8_t *)map_buffer;
			auto ss = cache_linesize < cache_srclinesize ? cache_linesize : cache_srclinesize;
			for (size_t i = 0; i < cache_height; i++) {
				memcpy(ptr + i * cache_srclinesize, data + i * cache_linesize, ss);
			}
		}
		map_buffer_ready = true;
	}
	data_mutex.unlock();

	emit update();
}

QOpenGLFramebufferObject *FBORenderer::createFramebufferObject(const QSize &size)
{
	if (!display) {
		gs_init_data info = {};
		info.cx = size.width();
		info.cy = size.height();
		info.format = GS_RGBA;
		info.zsformat = GS_ZS_NONE;

		display = obs_display_create(&info, GREY_COLOR_BACKGROUND, nullptr, FBORenderer::textureDataCallback, this, to_texture);
		obs_display_add_draw_callback(display, OBSRender, this);
	} else {
		obs_display_resize(display, size.width(), size.height());
	}
	QOpenGLFramebufferObjectFormat format;
	format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	format.setSamples(4);
	return new QOpenGLFramebufferObject(size, format);
}

void FBORenderer::OBSRender(void *data, uint32_t cx, uint32_t cy)
{
	FBORenderer *render = reinterpret_cast<FBORenderer *>(data);
	auto source = render->source;

	uint32_t targetCX;
	uint32_t targetCY;
	int x, y;
	int newCX, newCY;
	float scale;

	if (source) {
		targetCX = std::max(obs_source_get_width(source), 1u);
		targetCY = std::max(obs_source_get_height(source), 1u);
	} else {
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);
		targetCX = ovi.base_width;
		targetCY = ovi.base_height;
	}

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));

	if (source)
		obs_source_video_render(source);
	else
		obs_render_main_texture();

	endRegion();
}

void FBORenderer::textureDataCallback(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height, void *param)
{
	FBORenderer *render = reinterpret_cast<FBORenderer *>(param);
	render->textureDataCallbackInternal(data, linesize, src_linesize, src_height);
}

ProjectorItem::ProjectorItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{

	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &ProjectorItem::update);
	timer->start(17);
}

QQuickFramebufferObject::Renderer *ProjectorItem::createRenderer() const
{
	auto render = new FBORenderer;
	//connect(render, &FBORenderer::update, this, &ProjectorItem::update);
	return render;
}
