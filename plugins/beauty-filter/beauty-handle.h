#pragma once

#include "BEFEffectGLContext.h"
#include "effect_manager/be_effect_handle.h"
#include "effect_manager/be_render.h"
#include "effect_manager/be_detector.h"
#include <obs-module.h>
#include "PBOReader.h"
#include <QMutex>
#include <QImage>
#include <QTimer>

class BeautyHandle {
public:
	enum GameStickerType {
		Bomb,
		Strawberry,
		GameStart,
		GameStop,
		None,
	};

	BeautyHandle(obs_source_t *context);
	~BeautyHandle();
	obs_source_frame *processFrame(obs_source_frame *frame);
	void updateBeautySettings(obs_data_t *beautySetting);
	void updateStrawberrySettings(obs_data_t *setting);

private:
	void initOpenGL();
	void initShader();
	void initStrawberryShader();
	void initVertexData();
	void initStrawVertexData();
	void clearGLResource();
	void createTextures(int w, int h);
	void deleteTextures();
	void freeResource();
	void createPBO(int w, int h);
	void deletePBO();
	void checkBeautySettings();
	void calcPosition(int &width, int &height, int w, int h);
	void updateStrawberryData(float width, float height, bef_ai_face_info info, bool fliph, bool flip);

private:
	obs_source_t *m_source;
	BEF::BEFEffectGLContext m_glctx;

	bool effectHandlerInited = false;
	bool beautyEnabled = false;
	EffectHandle *m_effectHandle = nullptr;
	BEDectector* m_dectector = nullptr;

	QMutex m_beautifySettingMutex;
	QMutex m_strawberryMutex;
	QList<QString> m_beautifySettings;

	uint32_t m_lastWidth = 0;
	uint32_t m_lastHeight = 0;

	GLuint m_shader;
	GLuint m_vao;
	GLuint m_vbo;
	GLuint m_fbo;
	GLuint m_strawberryShader;
	GLuint m_strawberryVao;
	GLuint m_strawberryVbo;
	PBOReader *m_reader = nullptr;
	GLuint m_backgroundTexture = 0;
	GLuint m_outputTexture = 0;
	GLuint m_outputTexture2 = 0;
	GLuint m_strawberryTexture = 0;
	GLuint m_bombTexture = 0;

	GameStickerType m_gameStickerType = None;
	QImage m_strawberry;
	QImage m_bomb;
	int m_curRegion = -1;
	quint64 m_gameStartTime;
};
