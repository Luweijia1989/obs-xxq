#pragma once

#include <obs.hpp>

#define DEFINE_PROPERTY(type, name)                                          \
                                                                             \
private:                                                                     \
	Q_PROPERTY(type name READ name WRITE set##name NOTIFY name##Changed) \
                                                                             \
private:                                                                     \
	type m_##name;                                                       \
                                                                             \
public:                                                                      \
	void set##name(const type &a)                                        \
	{                                                                    \
		if (a != m_##name) {                                         \
			m_##name = a;                                        \
			emit name##Changed();                                \
		}                                                            \
	}                                                                    \
                                                                             \
public:                                                                      \
	type name() const { return m_##name; }                               \
                                                                             \
public:                                                                      \
Q_SIGNALS:                                                                   \
	void name##Changed();

#define GREY_COLOR_BACKGROUND 0xFF4C4C4C

#include <QQuickFramebufferObject>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QMutex>

typedef BOOL(WINAPI *WGLSETRESOURCESHAREHANDLENVPROC)(void *, HANDLE);
typedef HANDLE(WINAPI *WGLDXOPENDEVICENVPROC)(void *);
typedef BOOL(WINAPI *WGLDXCLOSEDEVICENVPROC)(HANDLE);
typedef HANDLE(WINAPI *WGLDXREGISTEROBJECTNVPROC)(HANDLE, void *, GLuint, GLenum, GLenum);
typedef BOOL(WINAPI *WGLDXUNREGISTEROBJECTNVPROC)(HANDLE, HANDLE);
typedef BOOL(WINAPI *WGLDXOBJECTACCESSNVPROC)(HANDLE, GLenum);
typedef BOOL(WINAPI *WGLDXLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);
typedef BOOL(WINAPI *WGLDXUNLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);

class FBORenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions {
public:
	FBORenderer(bool share_texture);
	~FBORenderer();
	void render() override;
	QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;

	static void displayRenderCallback(void *data, uint32_t cx, uint32_t cy);
	static void textureDataCallback(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height, void *param);

	virtual void displayRender(uint32_t cx, uint32_t cy) = 0;

private:
	void textureDataCallbackInternal(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height);

	void init_gl();
	void init_shader();
	void init_nv_functions();
	bool init_gl_texture();
	void release_gl_texture();

private:
	QOpenGLShaderProgram program;
	QOpenGLVertexArrayObject vao;
	QOpenGLBuffer vbo;
	QOpenGLBuffer ebo;

	bool dx_interop_available = false;

	GLuint texture = 0;
	GLuint backup_texture = 0;
	GLuint unpack_buffer = 0;
	int pbo_size = 0;
	obs_display_t *display = nullptr;

	QMutex data_mutex;
	uint32_t cache_linesize = 0;
	uint32_t cache_srclinesize = 0;
	uint32_t cache_height = 0;
	bool size_changed = false;
	bool map_buffer_ready = false;
	void *map_buffer = nullptr;

	HANDLE gl_device = INVALID_HANDLE_VALUE;
	HANDLE gl_dxobj = INVALID_HANDLE_VALUE;
	void *last_renderer_texture = nullptr;
};

class ProjectorItem : public QQuickFramebufferObject {
	Q_OBJECT
public:
	ProjectorItem(QQuickItem *parent = nullptr);
	~ProjectorItem();

private:
	uint32_t backgroundColor = GREY_COLOR_BACKGROUND;

	static QList<ProjectorItem *> items;
	static QTimer *timer;
};

class LinkRenderer : public FBORenderer {
public:
	LinkRenderer(bool share_texture = false);
	virtual void displayRender(uint32_t cx, uint32_t cy);
};

class LinkProjectorItem : public ProjectorItem {
	Q_OBJECT
public:
	LinkProjectorItem(QQuickItem *parent = nullptr);

	QQuickFramebufferObject::Renderer *createRenderer() const override;
};
