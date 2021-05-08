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
#include <QOpenGLTexture>

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
	bool doFaceDetect(bool needBeauty, bool needSticker, unsigned char *inputBuffer, int width, int height, bool flipv);
	void flipFaceDetect(bool flipv, bool fliph, int width, int height);
	QOpenGLTexture *doBeautify(QOpenGLTexture *srcTexture, QOpenGLTexture *beautify, QOpenGLTexture *makeup, QOpenGLTexture *filter, int width, int height, bool fliph, bool flipv);
	bool initSenseTimeEnv();
	bool stInited() { return m_stInited; }
	void freeStResource();
	int addSticker(QString sticker);
	void removeSticker(int id);
	bool clearSticker();
	const st_mobile_human_action_t &detectResult() { return m_result; }
	void updateBeautifyParam(const QJsonObject &setting);

private:
	st_handle_t m_stHandler = NULL;
	st_handle_t m_handleSticker = NULL;
	st_handle_t h_beautify = NULL;
	st_handle_t h_makeup = NULL;
	st_handle_t h_filter = NULL;
	st_mobile_human_action_t m_result = {0};
	unsigned long long m_beautyDetectConfig = 0x00FFFFFF;
	unsigned long long m_stickerDetectConfig = 0x00FFFFFF;
	bool m_stInited = false;
	std::vector<std::string> Packages;
	QString m_currentStickerId;
	QMap<QString, int> m_beautifySettingMap;
	QMap<QString, int> m_makeupSettingMap;
	bool m_hasFilter = false;
};
