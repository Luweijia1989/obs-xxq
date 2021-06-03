#pragma once

#include "obs.h"
#include "obs-module.h"
#include <QString>

class WebCapture {
public:
	WebCapture(QString memoryName);
	~WebCapture();

	void createTexture(int w, int h);
	void updateTextureData(uint8_t *data);

	obs_source_t *m_source;
	gs_texture_t *m_imageTexture;
	int width;
	int height;
	bool textureCreated = false;
};
