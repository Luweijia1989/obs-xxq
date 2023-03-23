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

class FBORenderer : public QObject, public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions {
	Q_OBJECT
public:
	FBORenderer();
	~FBORenderer();
	void render() override;
	QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;

	static void OBSRender(void *data, uint32_t cx, uint32_t cy);
	static void textureDataCallback(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height, void *param);

private:
	void textureDataCallbackInternal(uint8_t *data, uint32_t linesize, uint32_t src_linesize, uint32_t src_height);

signals:
	void update();

private:
	QOpenGLShaderProgram program;
	QOpenGLVertexArrayObject vao;
	QOpenGLBuffer vbo;
	QOpenGLBuffer ebo;

	GLuint texture = 0;
	GLuint unpack_buffer = 0;
	obs_display_t *display = nullptr;
	obs_source_t *source = nullptr;

	QMutex data_mutex;
	uint8_t *texture_data = nullptr;
	uint32_t cache_linesize = 0;
	uint32_t cache_srclinesize = 0;
	uint32_t cache_height = 0;
	bool size_changed = false;
	bool map_buffer_ready = false;
	void *map_buffer = nullptr;
};

class ProjectorItem : public QQuickFramebufferObject {
	Q_OBJECT

public:
	ProjectorItem(QQuickItem *parent = nullptr);

	QQuickFramebufferObject::Renderer *createRenderer() const override;

private:
	uint32_t backgroundColor = GREY_COLOR_BACKGROUND;
};
