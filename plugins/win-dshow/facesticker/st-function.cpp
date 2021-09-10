#include "st-function.h"
#include <QOpenGLFunctions>
#include <QDebug>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include "st-helper.h"

static QString NewAddDefaultSetting = u8R"({
	"滤镜版本" : "#",
	"美白" : "0",
	"红润" : "0",
	"磨皮" : "0",
	"瘦脸" : "0",
	"大眼" : "0",
	"小脸" : "0",
	"窄脸" : "0",
	"圆眼" : "0",
	"瘦脸型" : "0",
	"下巴" : "0",
	"苹果肌" : "0",
	"瘦鼻翼" : "0",
	"长鼻" : "0",
	"侧脸隆鼻" : "0",
	"嘴形" : "0",
	"缩人中" : "0",
	"开眼角" : "0",
	"亮眼" : "0",
	"祛黑眼圈" : "0",
	"祛法令纹" : "0",
	"白牙" : "0",
	"瘦颧骨" : "0",
	"开外眼角" : "0",
	"饱和度" : "0",
	"锐化" : "0",
	"清晰度" : "0",
	"对比度" : "0",
	"口红" : "#",
	"腮红" : "#",
	"修容" : "#",
	"眉毛" : "#",
	"眼线" : "#",
	"眼影" : "#",
	"眼睫毛" : "#"
})";

STFunction::STFunction()
{
	m_makeupSettingMap.insert(u8"口红", ST_MAKEUP_TYPE_LIP);
	m_makeupSettingMap.insert(u8"腮红", ST_MAKEUP_TYPE_FACE);
	m_makeupSettingMap.insert(u8"修容", ST_MAKEUP_TYPE_NOSE);
	m_makeupSettingMap.insert(u8"眉毛", ST_MAKEUP_TYPE_BROW);
	m_makeupSettingMap.insert(u8"眼线", ST_MAKEUP_TYPE_EYELINER);
	m_makeupSettingMap.insert(u8"眼影", ST_MAKEUP_TYPE_EYE);
	m_makeupSettingMap.insert(u8"眼睫毛", ST_MAKEUP_TYPE_EYELASH);

	m_beautifySettingMap.insert(u8"磨皮", ST_BEAUTIFY_SMOOTH_STRENGTH);
	m_beautifySettingMap.insert(u8"美白", ST_BEAUTIFY_WHITEN_STRENGTH);
	m_beautifySettingMap.insert(u8"瘦脸", ST_BEAUTIFY_SHRINK_FACE_RATIO);
	m_beautifySettingMap.insert(u8"红润", ST_BEAUTIFY_REDDEN_STRENGTH);
	m_beautifySettingMap.insert(u8"饱和度", ST_BEAUTIFY_SATURATION_STRENGTH);

	m_beautifySettingMap.insert(u8"大眼", ST_BEAUTIFY_ENLARGE_EYE_RATIO);
	m_beautifySettingMap.insert(u8"圆眼", ST_BEAUTIFY_ROUND_EYE_RATIO);
	m_beautifySettingMap.insert(u8"眼距", ST_BEAUTIFY_3D_EYE_DISTANCE_RATIO);
	m_beautifySettingMap.insert(u8"开眼角", ST_BEAUTIFY_3D_OPEN_CANTHUS_RATIO);
	m_beautifySettingMap.insert(u8"亮眼", ST_BEAUTIFY_3D_BRIGHT_EYE_RATIO);
	m_beautifySettingMap.insert(u8"开外眼角", ST_BEAUTIFY_3D_OPEN_EXTERNAL_CANTHUS_RATIO);
	m_beautifySettingMap.insert(u8"眼睛角度", ST_BEAUTIFY_3D_EYE_ANGLE_RATIO);

	m_beautifySettingMap.insert(u8"小脸", ST_BEAUTIFY_SHRINK_JAW_RATIO);
	m_beautifySettingMap.insert(u8"窄脸", ST_BEAUTIFY_NARROW_FACE_STRENGTH);
	m_beautifySettingMap.insert(u8"对比度", ST_BEAUTIFY_CONTRAST_STRENGTH);
	m_beautifySettingMap.insert(u8"瘦脸型", ST_BEAUTIFY_3D_THIN_FACE_SHAPE_RATIO);
	m_beautifySettingMap.insert(u8"瘦颧骨", ST_BEAUTIFY_SHRINK_CHEEKBONE_RATIO);
	m_beautifySettingMap.insert(u8"下巴", ST_BEAUTIFY_3D_CHIN_LENGTH_RATIO);
	m_beautifySettingMap.insert(u8"瘦鼻翼", ST_BEAUTIFY_3D_NARROW_NOSE_RATIO);
	m_beautifySettingMap.insert(u8"长鼻", ST_BEAUTIFY_3D_NOSE_LENGTH_RATIO);
	m_beautifySettingMap.insert(u8"缩人中", ST_BEAUTIFY_3D_PHILTRUM_LENGTH_RATIO);
	m_beautifySettingMap.insert(u8"苹果肌", ST_BEAUTIFY_3D_APPLE_MUSLE_RATIO);
	m_beautifySettingMap.insert(u8"侧脸隆鼻", ST_BEAUTIFY_3D_PROFILE_RHINOPLASTY_RATIO);
	m_beautifySettingMap.insert(u8"祛黑眼圈", ST_BEAUTIFY_3D_REMOVE_DARK_CIRCLES_RATIO);
	m_beautifySettingMap.insert(u8"祛法令纹", ST_BEAUTIFY_3D_REMOVE_NASOLABIAL_FOLDS_RATIO);
	m_beautifySettingMap.insert(u8"白牙", ST_BEAUTIFY_3D_WHITE_TEETH_RATIO);
	m_beautifySettingMap.insert(u8"锐化", ST_BEAUTIFY_SHARPEN_STRENGTH);
	m_beautifySettingMap.insert(u8"嘴形", ST_BEAUTIFY_3D_MOUTH_SIZE_RATIO);
}

STFunction::~STFunction() {}

bool STFunction::initSenseTimeEnv()
{
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

	QJsonDocument jd = QJsonDocument::fromJson(NewAddDefaultSetting.toUtf8());
	updateBeautifyParam(jd.object());

	unsigned long long beau_config = 0, makeup_config = 0;
	ret = st_mobile_beautify_get_detect_config(h_beautify, &beau_config);
	assert(ret == ST_OK);
	ret = st_mobile_makeup_get_trigger_action(h_makeup, &makeup_config);
	assert(ret == ST_OK);
	m_beautyDetectConfig = beau_config | makeup_config |
		ST_MOBILE_DETECT_MOUTH_PARSE |
		ST_MOBILE_DETECT_EXTRA_FACE_POINTS;

	ret = st_mobile_sticker_create(&m_handleSticker);
	if (ret != ST_OK) {
		qDebug() << "fail to init sticker handle: " << ret;
		return false;
	}
	m_stInited = true;
	return true;
}

void STFunction::updateBeautifyParam(const QJsonObject &setting)
{
	st_result_t ret = ST_OK;

	for (auto iter = setting.begin(); iter != setting.end(); iter++) {
		QString key = iter.key();
		QString value = iter.value().toString();
		if (key == u8"滤镜版本") {
			QStringList li = value.split('#');
			if (li.size() > 1) {
				QString filterPath = "";
				if (!li.at(0).isEmpty()) {
					filterPath = QString(u8"%1/%2.model").arg(QString::fromStdString(g_FilterDir)).arg(li.at(0));
					m_hasFilter = true;
				}
				else
					m_hasFilter = false;

				QByteArray filterData;
				QFile filterFile(filterPath);
				if (filterFile.open(QFile::ReadOnly)) {
					filterData = filterFile.readAll();
					filterFile.close();
				}
				ret = st_mobile_gl_filter_set_style_from_buffer(h_filter, (const unsigned char *)filterData.data(), filterData.size());
				assert(ret == ST_OK);
				ret = st_mobile_gl_filter_set_param(h_filter, ST_GL_FILTER_STRENGTH, li.at(1).toInt() / 100.0);
				assert(ret == ST_OK);
			}
		}
		else {
			bool isMakeup = value.contains('#');
			if (isMakeup) {
				QStringList li = value.split('#');
				if (li.size() > 1) {
					QString makeupPath = "";
					if (!li.at(0).isEmpty()) {
						makeupPath = QString(u8"%1/%2.zip").arg(QString::fromStdString(g_MakeupDir)).arg(li.at(0));
						QByteArray makeupData;
						QFile makeupFile(makeupPath);
						if (makeupFile.open(QFile::ReadOnly)) {
							makeupData = makeupFile.readAll();
							makeupFile.close();

							int mid = 0;
							ret = st_mobile_makeup_set_makeup_for_type_from_buffer(h_makeup, (st_makeup_type)m_makeupSettingMap.value(key), (const unsigned char *)makeupData.data(), makeupData.size(), &mid);
							assert(ret == ST_OK);
							st_mobile_makeup_set_strength_for_type(h_makeup, (st_makeup_type)m_makeupSettingMap.value(key), li.at(1).toInt() / 100.0);
							m_makeupInUse.insert(key, mid);
						}
					} else {
						if (m_makeupInUse.contains(key)) {
							st_mobile_makeup_remove_makeup(h_makeup, m_makeupInUse.value(key));
							m_makeupInUse.remove(key);
						}
					}

				}
			}
			else {
				st_mobile_beautify_setparam(h_beautify, (st_beautify_type)m_beautifySettingMap.value(key), value.toInt()/100.0);
			}
		}
	}
}

void STFunction::freeStResource()
{
	if (m_stHandler)
		st_mobile_human_action_destroy(m_stHandler);
	if (m_handleSticker)
		st_mobile_sticker_destroy(m_handleSticker);
	if (h_beautify)
		st_mobile_beautify_destroy(h_beautify);
	if (h_makeup)
		st_mobile_makeup_destroy(h_makeup);
	if (h_filter)
		st_mobile_gl_filter_destroy(h_filter);
}

int STFunction::addSticker(QString sticker)
{
	int id = 0;
	int ret = st_mobile_sticker_add_package(m_handleSticker, sticker.toStdString().c_str(), &id);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_stickerDetectConfig);
	return id;
}

void STFunction::removeSticker(int id)
{
	st_mobile_sticker_remove_package(m_handleSticker, id);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_stickerDetectConfig);
}

bool STFunction::clearSticker()
{
	st_mobile_sticker_clear_packages(m_handleSticker);
	st_mobile_sticker_get_trigger_action(m_handleSticker, &m_stickerDetectConfig);
	return true;
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
		false,
		&m_result,
		nullptr,
		output);

	return ret == ST_OK;
}

bool STFunction::doFaceDetect(bool needBeauty, bool needSticker, unsigned char *inputBuffer, int width, int height, bool flipv)
{
	long long detectConfig = ST_MOBILE_FACE_DETECT | ST_MOBILE_MOUTH_AH;
	if (needBeauty)
		detectConfig |= m_beautyDetectConfig;
	if (needSticker)
		detectConfig |= m_stickerDetectConfig;

	int ret = st_mobile_human_action_detect(
		m_stHandler,
		inputBuffer,
		ST_PIX_FMT_RGBA8888,
		width,
		height,
		width * 4,
		flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		detectConfig,
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

QOpenGLTexture *STFunction::doBeautify(QOpenGLTexture *srcTexture, QOpenGLTexture *beautify, QOpenGLTexture *makeup, QOpenGLTexture *filter, int width,
			    int height, bool fliph, bool flipv)
{
	QOpenGLTexture *tex = nullptr;
	int ret = st_mobile_beautify_process_texture(h_beautify,
		srcTexture->textureId(),
		width,
		height,
		flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
		&m_result,
		beautify->textureId(),
		&m_result);
	if (!m_hasFilter) {
		ret = st_mobile_makeup_process_texture(h_makeup,
			beautify->textureId(),
			width,
			height,
			flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
			&m_result,
			makeup->textureId());
		tex = makeup;
	} else {
		ret = st_mobile_makeup_process_texture(h_makeup,
			beautify->textureId(),
			width,
			height,
			flipv ? ST_CLOCKWISE_ROTATE_180 : ST_CLOCKWISE_ROTATE_0,
			&m_result,
			makeup->textureId());
		ret = st_mobile_gl_filter_process_texture(h_filter,
			makeup->textureId(),
			width,
			height,
			filter->textureId());
		tex = filter;
	} 
	return tex;
}
