#pragma once

#include "BEFEffectGLContext.h"
#include "effect_manager/be_effect_handle.h"
#include "effect_manager/be_render.h"
#include <obs-module.h>
#include "PBOReader.h"
#include <QMutex>

class BeautyHandle {
public:
	BeautyHandle(obs_source_t *context);
	~BeautyHandle();
	obs_source_frame *processFrame(obs_source_frame *frame);

private:
	void initOpenGL();
	void initShader();
	void initVertexData();
	void clearGLResource();
	void createTextures(int w, int h);
	void deleteTextures();
	void freeResource();
	void createPBO(int w, int h);
	void deletePBO();
	bool checkBeautySettings();

private:
	obs_source_t *m_source;
	BEF::BEFEffectGLContext m_glctx;

	bool effectHandlerInited = false;
	EffectHandle *m_effectHandle = nullptr;

	QMutex m_beautifySettingMutex;
	QList<QString> m_beautifySettings;

	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	GLuint m_shader;
	GLuint m_vao;
	GLuint m_vbo;
	GLuint m_fbo;
	PBOReader *m_reader = nullptr;
	GLuint m_backgroundTexture = 0;
	GLuint m_outputTexture = 0;
	GLuint m_outputTexture2 = 0;
};
