#include "qt-opengl-display.hpp"
#include <QDebug>
#include <qopenglframebufferobject.h>
#include <QQuickWindow>
#include <QTimer>

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
}

FBORenderer::~FBORenderer()
{
	obs_display_remove_draw_callback(display, OBSRender, this);
	obs_display_destroy(display);

	if (texture)
		delete texture;

	if (texture_data)
		bfree(texture_data);
}

void FBORenderer::render()
{
	glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		QMutexLocker locker(&data_mutex);
		if (texture_data) {
			if (size_changed) {
				size_changed = false;
				if (texture)
					delete texture;

				texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
				texture->create();
				texture->setSize(cache_linesize / 4, cache_height);
				texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
				texture->allocateStorage();
			}

			texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, texture_data);
		}
	}
	if (texture)
		texture->bind();

	program.bind();

	vao.bind();

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	vao.release();
	program.release();
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

void FBORenderer::textureDataCallbackInternal(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height)
{
	QMutexLocker locker(&data_mutex);
	if (data) {
		if (src_linesize != cache_linesize || src_height != cache_height) {
			size_changed = true;
			cache_linesize = src_linesize;
			cache_height = src_height;

			if (texture_data)
				bfree(texture_data);

			texture_data = (uint8_t *)bmalloc(cache_linesize * cache_height);
		}

		uint8_t *in_ptr = data;
		uint8_t *out_ptr = texture_data;

		/* if the line sizes match, do a single copy */
		if (cache_linesize == linesize) {
			//memcpy(out_ptr, in_ptr, cache_linesize * cache_height);
		} else {
			for (size_t y = 0; y < cache_height; y++) {
				memcpy(out_ptr, in_ptr, cache_linesize);
				in_ptr += linesize;
				out_ptr += cache_linesize;
			}
		}
	}
}

ProjectorItem::ProjectorItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{
	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &ProjectorItem::update);
	timer->start(40);
}

QQuickFramebufferObject::Renderer *ProjectorItem::createRenderer() const
{
	auto render = new FBORenderer;
	return render;
}
