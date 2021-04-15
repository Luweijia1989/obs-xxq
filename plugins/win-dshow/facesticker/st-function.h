#pragma once

#include "st_mobile_sticker.h"
#include "st_mobile_human_action.h"
#include "st_mobile_beautify.h"
#include "st_mobile_makeup.h"
#include "st_mobile_filter.h"
#include <map>
#include <vector>
#include <QString>
#include <QMap>

typedef struct BeautifyParamData {
	QString name;
	st_beautify_type type;
	float value;
} BeautifyParamData;

typedef struct MakeupParamData {
	QString name;
	QString path;
	st_makeup_type type;
	float value;
} MakeupParamData;

class STFunction {
public:
	STFunction();
	~STFunction();

	bool doFaceSticker(unsigned int input, unsigned int output, int width, int height, bool fliph, bool flipv);
	bool doFaceDetect(unsigned char *inputBuffer, int width, int height, bool flipv);
	void flipFaceDetect(bool flipv, bool fliph, int width, int height);
	bool initSenseTimeEnv();
	bool stInited() { return m_stInited; }
	void freeSticker();
	void freeFaceHandler();
	int addSticker(QString sticker);
	void removeSticker(int id);
	bool clearSticker();
	const st_mobile_human_action_t &detectResult() { return m_result; }

private:
	void iterateBeautifyJsons();
	void loadBeautifyParam(QString name);

private:
	st_handle_t m_stHandler = NULL;
	st_handle_t m_handleSticker = NULL;
	st_handle_t h_beautify = NULL;
	st_handle_t h_makeup = NULL;
	st_handle_t h_filter = NULL;
	std::vector<BeautifyParamData> gBeauParams;
	std::vector<MakeupParamData> gMakeupParam;
	QMap<QString, double> m_filers;
	QMap<QString, QString> m_beautifyJsons;
	st_mobile_human_action_t m_result = {0};
	unsigned long long m_detectConfig = 0x00FFFFFF;
	bool m_stInited = false;
	std::vector<std::string> Packages;
	QString m_currentStickerId;
};
