#pragma once

#include "obs.h"
#include "obs-module.h"
#include "RPCService.h"
#include <QSharedMemory>

class WebCapture;

class WebCaptureRPC : public RPCService {
	Q_OBJECT
public:
	WebCaptureRPC(QString name, WebCapture *cap);

	DEF_PROCESS_INVOKE(WebCaptureRPC)

protected:
	void paramParse(int actionType, const QString &strParam);

private:
	WebCapture *m_capture;
	QSharedMemory m_memory;
	bool m_created = false;
};

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
	WebCaptureRPC *m_rpc;
};
