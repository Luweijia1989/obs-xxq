#pragma once

class CAirServerCallback;
class AirPlayServer;

class CAirServer {
public:
	CAirServer();
	~CAirServer();

public:
	bool start(AirPlayServer *s);
	void stop();
	float setVideoScale(float fRatio);

private:
	CAirServerCallback *m_pCallback;
	void *m_pServer;
};
