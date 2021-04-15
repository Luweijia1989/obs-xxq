#include "st-function.h"
#include <QOpenGLFunctions>
#include <QDebug>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include "st-helper.h"

STFunction::STFunction() {}

STFunction::~STFunction() {}

bool STFunction::initSenseTimeEnv()
{
	iterateBeautifyJsons();
	if (!m_beautifyJsons.isEmpty())
		loadBeautifyParam(m_beautifyJsons.firstKey());

	unsigned int create_config = 0;
	create_config |= ST_MOBILE_HUMAN_ACTION_DEFAULT_CONFIG_VIDEO;
	st_result_t ret = ST_OK;
	std::string face_model_name = g_ModelDir + face_model;
	std::string ModelExtraPath = g_ModelDir + face_extra_model;
	std::string ModelMouthPath = g_ModelDir + mouthocc_model;  // 嘴唇遮挡模型
	std::string eyeball_model_path = g_ModelDir + eyeball_model;

	ret = st_mobile_human_action_create(face_model_name.c_str(),
					    create_config, &m_stHandler);
	if (ret != ST_OK) {
		qDebug() << "fail to init human_action handle: " << ret;
		return false;
	}

	ret = st_mobile_human_action_add_sub_model(m_stHandler, ModelExtraPath.c_str());
	assert(ret == ST_OK);
	ret = st_mobile_human_action_add_sub_model(m_stHandler, ModelMouthPath.c_str());
	assert(ret == ST_OK);
	ret = st_mobile_human_action_add_sub_model(m_stHandler, eyeball_model_path.c_str());
	if (ret != ST_OK) {
		qDebug() << "fail to load eyeball model: " << ret;
	}

	ret = st_mobile_beautify_create(&h_beautify);
	if (ret != ST_OK) {
		qDebug() << "fail to init beautify handle";
		return false;
	}
	ret = st_mobile_makeup_create(&h_makeup);
	assert(ret == ST_OK);
	ret = st_mobile_gl_filter_create(&h_filter);
	assert(ret == ST_OK);

	//磨皮模式设置
	ret = st_mobile_beautify_setparam(h_beautify, ST_BEAUTIFY_SMOOTH_MODE, 2.0f);
	assert(ret == ST_OK);
	////默认美颜参数设置
	for (int i = 0; i < gBeauParams.size(); i++) {
		ret = st_mobile_beautify_setparam(h_beautify, gBeauParams[i].type, gBeauParams[i].value);
		assert(ret == ST_OK);
	}

	ret = st_mobile_sticker_create(&m_handleSticker);
	if (ret != ST_OK) {
		qDebug() << "fail to init sticker handle: " << ret;
		return false;
	}
	m_stInited = true;
	return true;
}

void STFunction::freeSticker()
{
	if (m_handleSticker)
		st_mobile_sticker_destroy(m_handleSticker);
}

void STFunction::freeFaceHandler()
{
	if (m_stHandler)
		st_mobile_human_action_destroy(m_stHandler);
}

int STFunction::addSticker(QString sticker)
{
	int id = 0;
	int ret = st_mobile_sticker_add_package(
		m_handleSticker, sticker.toStdString().c_str(), &id);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_detectConfig);
	return id;
}

void STFunction::removeSticker(int id)
{
	st_mobile_sticker_remove_package(m_handleSticker, id);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_detectConfig);
}

bool STFunction::clearSticker()
{
	st_mobile_sticker_clear_packages(m_handleSticker);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_detectConfig);
	return true;
}

void STFunction::iterateBeautifyJsons()
{
	QDir dir(QString::fromStdString(g_JsonDir));
	auto files = dir.entryInfoList(QDir::Files);
	for (auto iter = files.begin(); iter != files.end(); iter++)
	{
		const QFileInfo &info = *iter;
		m_beautifyJsons.insert(info.baseName(), info.filePath());
	}
}

void STFunction::loadBeautifyParam(QString name)
{
	if (!m_beautifyJsons.contains(name))
		return;

	QFile f(m_beautifyJsons.value(name));
	if (!f.open(QFile::ReadOnly))
		return;

	QJsonParseError error;
	QJsonDocument jd = QJsonDocument::fromJson(f.readAll(), &error);
	f.close();

	QJsonObject json = jd.object();
	QJsonArray arr = json["AllFilterName"].toArray();
	m_filers.clear();
	for (int i = 0; i < arr.size(); i++)
	{
		auto one = arr.at(i).toObject();
		m_filers.insert(QString("%1/%2.model").arg(QString::fromStdString(g_FilterDir)).arg(one["name"].toString()), one["value"].toDouble());
	}

	gBeauParams.clear();
	QJsonArray beparam = json["BeauParams"].toArray();
	for (int i = 0; i < beparam.size(); i++)
	{
		auto one = beparam.at(i).toObject();
		auto name = one["name"].toString();
		if (name.isEmpty())
			continue;

		gBeauParams.push_back({ name, st_beautify_type(one["beautify_type"].toInt()), (float)one["value"].toDouble() });
	}

	gMakeupParam.clear();
	QJsonArray makeupparam = json["makeup"].toArray();
	for (int i = 0; i < makeupparam.size(); i++)
	{
		auto one = makeupparam.at(i).toObject();
		auto name = one["name"].toString();
		if (name.isEmpty())
			continue;

		QString path = QString("%1/%2.zip").arg(QString::fromStdString(g_MakeupDir)).arg(name);
		gMakeupParam.push_back({ name, path, st_makeup_type(one["type"].toInt()), (float)one["value"].toDouble() });
	}
}

bool STFunction::doFaceSticker(unsigned int input, unsigned int output, int width, int height, bool fliph, bool flipv)
{
	int ret = st_mobile_sticker_process_texture(
		m_handleSticker,
		input,
		width,
		height,
		flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		ST_CLOCKWISE_ROTATE_0,
		flipv ^ fliph,
		&m_result,
		nullptr,
		output);

	return ret == ST_OK;
}

bool STFunction::doFaceDetect(unsigned char *inputBuffer, int width, int height, bool flipv)
{
	int ret = st_mobile_human_action_detect(
		m_stHandler,
		inputBuffer,
		ST_PIX_FMT_RGBA8888,
		width,
		height,
		width * 4,
		flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		ST_MOBILE_FACE_DETECT | ST_MOBILE_MOUTH_AH,
		&m_result);
	return ret == ST_OK;
}

void STFunction::flipFaceDetect(bool flipv, bool fliph, int width, int height)
{
	if (flipv)
	{
		st_mobile_human_action_rotate(width, height, ST_CLOCKWISE_ROTATE_180, false, &m_result);
		st_mobile_human_action_mirror(width, &m_result);
	}

	if (fliph)
		st_mobile_human_action_mirror(width, &m_result);
}
