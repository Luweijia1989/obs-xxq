#include "qt-opengl-display.hpp"
#include <QDebug>
#include <qapplication.h>
#include <qopenglframebufferobject.h>
#include <QQuickWindow>
#include <QTimer>
#include <QDateTime>
#include <qopenglcontext.h>
#include <qopenglextrafunctions.h>
#include <qelapsedtimer.h>

#include "display-helpers.hpp"

static const char *vertex_str = R"(#version 330 core
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

static const char *fragment_str = R"(#version 330 core
                                out vec4 FragColor;

                                in vec3 ourColor;
                                in vec2 TexCoord;

                                // texture sampler
                                uniform sampler2D texture1;

                                void main()
                                {
                                    FragColor = texture(texture1, TexCoord);
                                })";

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

static WGLSETRESOURCESHAREHANDLENVPROC jimglDXSetResourceShareHandleNV = NULL;
static WGLDXOPENDEVICENVPROC jimglDXOpenDeviceNV = NULL;
static WGLDXCLOSEDEVICENVPROC jimglDXCloseDeviceNV = NULL;
static WGLDXREGISTEROBJECTNVPROC jimglDXRegisterObjectNV = NULL;
static WGLDXUNREGISTEROBJECTNVPROC jimglDXUnregisterObjectNV = NULL;
static WGLDXOBJECTACCESSNVPROC jimglDXObjectAccessNV = NULL;
static WGLDXLOCKOBJECTSNVPROC jimglDXLockObjectsNV = NULL;
static WGLDXUNLOCKOBJECTSNVPROC jimglDXUnlockObjectsNV = NULL;

static bool nv_capture_available = false;

bool dx_gl_interop_available = false;

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

FBORenderer::FBORenderer(bool share_texture) : dx_interop_available(share_texture)
{
	static bool set_dx_interop_value = false;
	if (!set_dx_interop_value) {
		set_dx_interop_value = true;
		obs_display_set_dxinterop_enabled(dx_interop_available);
	}
	pbo_size = 1920 * 1080 * 4;

	if (dx_interop_available)
		init_nv_functions();

	init_gl();
}

FBORenderer::~FBORenderer()
{
	obs_display_remove_draw_callback(display, displayRenderCallback, this);
	obs_display_destroy(display);

	if (unpack_buffer) {
		if (map_buffer) {
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
			QOpenGLContext::currentContext()->extraFunctions()->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			map_buffer = nullptr;
		}

		glDeleteBuffers(1, &unpack_buffer);
		unpack_buffer = 0;
	}

	if (texture) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}

	if (backup_texture) {
		glDeleteTextures(1, &backup_texture);
		backup_texture = 0;
	}

	if (dx_interop_available)
		release_gl_texture();
}

void FBORenderer::init_gl()
{
	initializeOpenGLFunctions();

	init_shader();

	if (!dx_interop_available) {
		glGenBuffers(1, &unpack_buffer);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size, 0, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
}

void FBORenderer::init_shader()
{

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

void FBORenderer::init_nv_functions()
{
	static bool nv_inited = false;

	if (nv_inited)
		return;

	nv_inited = true;
	jimglDXSetResourceShareHandleNV = (WGLSETRESOURCESHAREHANDLENVPROC)wglGetProcAddress("wglDXSetResourceShareHandleNV");
	jimglDXOpenDeviceNV = (WGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
	jimglDXCloseDeviceNV = (WGLDXCLOSEDEVICENVPROC)wglGetProcAddress("wglDXCloseDeviceNV");
	jimglDXRegisterObjectNV = (WGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
	jimglDXUnregisterObjectNV = (WGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
	jimglDXObjectAccessNV = (WGLDXOBJECTACCESSNVPROC)wglGetProcAddress("wglDXObjectAccessNV");
	jimglDXLockObjectsNV = (WGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
	jimglDXUnlockObjectsNV = (WGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");

	nv_capture_available = !!jimglDXSetResourceShareHandleNV && !!jimglDXOpenDeviceNV && !!jimglDXCloseDeviceNV && !!jimglDXRegisterObjectNV &&
			       !!jimglDXUnregisterObjectNV && !!jimglDXObjectAccessNV && !!jimglDXLockObjectsNV && !!jimglDXUnlockObjectsNV;

	if (nv_capture_available)
		qDebug("Shared-texture OpenGL capture available");
}

bool FBORenderer::init_gl_texture()
{
	obs_enter_graphics();
	gl_device = jimglDXOpenDeviceNV(gs_get_device_obj());
	if (!gl_device) {
		qDebug("gl_shtex_init_gl_tex: failed to open device");
		obs_leave_graphics();
		return false;
	}

	glGenTextures(1, &texture);

	auto tex = obs_display_get_texture(display);
	gl_dxobj = jimglDXRegisterObjectNV(gl_device, gs_texture_get_obj(tex), texture, GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
	if (!gl_dxobj) {
		qDebug("gl_shtex_init_gl_tex: failed to register object");
		obs_leave_graphics();
		return false;
	}

	obs_leave_graphics();
	return true;
}

void FBORenderer::release_gl_texture()
{
	if (texture) {
		jimglDXLockObjectsNV(gl_device, 1, &gl_dxobj);
		glDeleteTextures(1, &texture);
		texture = 0;
		jimglDXUnlockObjectsNV(gl_device, 1, &gl_dxobj);
	}

	if (gl_dxobj) {
		jimglDXUnregisterObjectNV(gl_device, gl_dxobj);
		gl_dxobj = INVALID_HANDLE_VALUE;
	}

	if (gl_device)
		jimglDXCloseDeviceNV(gl_device);
}

void FBORenderer::render()
{
	if (dx_interop_available) {
		obs_enter_graphics();
		auto tex = obs_display_get_texture(display);
		if (tex && tex != last_renderer_texture) {
			if (last_renderer_texture)
				release_gl_texture();

			init_gl_texture();

			last_renderer_texture = tex;
		}
		obs_leave_graphics();
	} else {
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

			if (!map_buffer) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
				map_buffer = QOpenGLContext::currentContext()->extraFunctions()->glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, pbo_size,
														  GL_MAP_WRITE_BIT);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			}
		}
	}

	if (!texture && !backup_texture) {
		return;
	}

	if (dx_interop_available) {
		obs_enter_graphics();
		jimglDXLockObjectsNV(gl_device, 1, &gl_dxobj);
	}

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

	if (dx_interop_available) {
		jimglDXUnlockObjectsNV(gl_device, 1, &gl_dxobj);
		obs_leave_graphics();
	}
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
}

QOpenGLFramebufferObject *FBORenderer::createFramebufferObject(const QSize &size)
{
	int w = 0, h = 0;
	if (dx_interop_available) {
		w = size.width();
		h = size.height();
	} else {
		w = (qreal)size.width() / qApp->devicePixelRatio();
		h = (qreal)size.height() / qApp->devicePixelRatio();
		w = qMin(w, 1920);
		h = qMin(h, 1080);
	}

	if (!display) {
		gs_init_data info = {};
		info.cx = w;
		info.cy = h;
		info.format = GS_RGBA;
		info.zsformat = GS_ZS_NONE;
		info.dx_interop_available = dx_interop_available;

		display = obs_display_create(&info, GREY_COLOR_BACKGROUND, nullptr, FBORenderer::textureDataCallback, this, to_texture);
		obs_display_add_draw_callback(display, displayRenderCallback, this);
	} else {
		obs_display_resize(display, w, h);
	}
	QOpenGLFramebufferObjectFormat format;
	format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	format.setSamples(4);
	return new QOpenGLFramebufferObject(size, format);
}

void FBORenderer::displayRenderCallback(void *data, uint32_t cx, uint32_t cy)
{
	FBORenderer *render = reinterpret_cast<FBORenderer *>(data);
	render->displayRender(cx, cy);
}

void FBORenderer::textureDataCallback(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height, void *param)
{
	FBORenderer *render = reinterpret_cast<FBORenderer *>(param);
	render->textureDataCallbackInternal(data, linesize, src_linesize, src_height);
}

QTimer *ProjectorItem::timer = nullptr;
QList<ProjectorItem *> ProjectorItem::items;

ProjectorItem::ProjectorItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{
	setAcceptedMouseButtons(Qt::AllButtons);
	setAcceptHoverEvents(true);

	if (!timer) {
		timer = new QTimer;
		connect(timer, &QTimer::timeout, this, [=]() {
			for (auto iter = items.begin(); iter != items.end(); iter++) {
				auto item = *iter;
				item->update();
			}
		});
	}

	if (!timer->isActive())
		timer->start(25);

	items.append(this);
}

ProjectorItem::~ProjectorItem()
{
	items.removeOne(this);
	if (items.isEmpty()) {
		timer->deleteLater();
		timer = nullptr;
	}
}

LinkRenderer::LinkRenderer(bool share_texture) : FBORenderer(share_texture) {}

void LinkRenderer::displayRender(uint32_t cx, uint32_t cy)
{
	uint32_t targetCX;
	uint32_t targetCY;
	int x, y;
	int newCX, newCY;
	float scale;

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	targetCX = ovi.base_width;
	targetCY = ovi.base_height;

	GetScaleAndCenterPos(targetCX, targetCY, cx, cy, x, y, scale);

	newCX = int(scale * float(targetCX));
	newCY = int(scale * float(targetCY));

	startRegion(x, y, newCX, newCY, 0.0f, float(targetCX), 0.0f, float(targetCY));

	obs_render_main_texture();

	endRegion();
}

LinkProjectorItem::LinkProjectorItem(QQuickItem *parent) : ProjectorItem(parent) {}

QQuickFramebufferObject::Renderer *LinkProjectorItem::createRenderer() const
{
	return new LinkRenderer(dx_gl_interop_available);
}

class QMLRegisterHelper
{
public:
	QMLRegisterHelper() {
		qmlRegisterType<LinkProjectorItem>("com.xxq", 1, 0, "LinkProjectorItem");
	}
};

static QMLRegisterHelper registerHelper;
