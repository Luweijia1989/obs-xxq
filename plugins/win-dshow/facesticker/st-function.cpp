#include "st-function.h"
#include <QOpenGLFunctions>
#include <QDebug>
#include "st-helper.h"

STFunction::STFunction() {}

STFunction::~STFunction() {}

bool STFunction::initSenseTimeEnv()
{
	unsigned int create_config = 0;
	create_config |= ST_MOBILE_HUMAN_ACTION_DEFAULT_CONFIG_VIDEO;
	st_result_t ret = ST_OK;
	std::string face_model_name = g_ModelDir + face_model;

	ret = st_mobile_human_action_create(face_model_name.c_str(),
					    create_config, &m_stHandler);
	if (ret != ST_OK) {
		qDebug() << "fail to init human_action handle: " << ret;
		return false;
	}

	st_mobile_human_action_setparam(
		m_stHandler, ST_HUMAN_ACTION_PARAM_BACKGROUND_BLUR_STRENGTH,
		35 / 100.0f);

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

bool STFunction::addSticker(QString sticker)
{
	int ret = st_mobile_sticker_add_package(
		m_handleSticker, sticker.toStdString().c_str(), NULL);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_detectConfig);
	return ret == ST_OK;
}

bool STFunction::clearSticker()
{
	st_mobile_sticker_clear_packages(m_handleSticker);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_detectConfig);
	return true;
}

bool STFunction::doFaceSticker(unsigned int input, unsigned int output,
			       int width, int height,
			       unsigned char *outputBuffer, bool flip)
{
	int ret = st_mobile_sticker_process_and_output_texture(
		m_handleSticker, input, width, height,
		flip ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		flip ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0, 0,
		&m_result, nullptr, output, outputBuffer, ST_PIX_FMT_NV12);

	return ret == ST_OK;
}

bool STFunction::doFaceDetect(unsigned char *inputBuffer, int width, int height,
			      bool flip)
{
	int ret = st_mobile_human_action_detect(
		m_stHandler, inputBuffer, ST_PIX_FMT_RGBA8888, width, height,
		width * 4,
		flip ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		m_detectConfig, &m_result);
	return ret == ST_OK;
}
