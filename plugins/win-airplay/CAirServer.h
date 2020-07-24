#pragma once

class CAirServerCallback;
class ScreenMirrorServer;

class CAirServer {
public:
	CAirServer();
	~CAirServer();

public:
	bool start(ScreenMirrorServer *s);
	void stop();
	float setVideoScale(float fRatio);

private:
	CAirServerCallback *m_pCallback;
	void *m_pServer;
};
