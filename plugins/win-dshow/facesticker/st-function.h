#pragma once

#include "st_mobile_sticker.h"
#include <map>
#include <vector>
#include <QString>

class STFunction {
public:
	STFunction();
	~STFunction();

	bool doFaceSticker(unsigned int input, unsigned int output, int width,
			   int height, unsigned char *outputBuffer);
	bool doFaceDetect(unsigned char *inputBuffer, int width, int height);
	bool initSenseTimeEnv();
	bool stInited() { return m_stInited; }
	void freeSticker();
	void freeFaceHandler();
	int addSticker(QString sticker);
	void removeSticker(int id);
	bool clearSticker();
	const st_mobile_human_action_t &detectResult() { return m_result; }

private:
	st_handle_t m_stHandler = NULL;
	st_handle_t m_handleSticker = NULL;
	st_mobile_human_action_t m_result = {0};
	unsigned long long m_detectConfig = 0x00FFFFFF;
	bool m_stInited = false;
	std::vector<std::string> Packages;
	QString m_currentStickerId;
};
