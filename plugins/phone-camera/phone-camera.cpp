#include "phone-camera.h"
#include <qfile.h>
#include <util/platform.h>

#include "ios-camera.h"
#include "android-camera.h"
#include "driver-helper.h"

#define PHONE_DEVICE_TYPE "device_type"
#define DEVICE_ID "device_id"

//#define DUMP_VIDEO

extern DriverHelper *driverHelper;

PhoneCamera::PhoneCamera(obs_data_t *settings, obs_source_t *source) : m_source(source)
{
#ifdef DUMP_VIDEO
	m_videodump.setFileName("dump.h264");
	m_videodump.open(QFile::ReadWrite);
#endif // DUMP_VIDEO

	video_format_get_parameters(VIDEO_CS_601, VIDEO_RANGE_DEFAULT, frame.color_matrix, frame.color_range_min, frame.color_range_max);

	m_androidCamera = new AndroidCamera;

	m_iOSCamera = new iOSCamera;
	connect(m_iOSCamera, &iOSCamera::updateDeviceList, this, [=](QMap<QString, QPair<QString, uint32_t>> devices) { m_iOSDevices = devices; });
	connect(m_iOSCamera, &iOSCamera::mediaData, this, &PhoneCamera::onMediaData, Qt::DirectConnection);
	connect(m_iOSCamera, &iOSCamera::mediaFinish, this, &PhoneCamera::onMediaFinish, Qt::DirectConnection);
	connect(m_androidCamera, &AndroidCamera::mediaData, this, &PhoneCamera::onMediaData, Qt::DirectConnection);
	connect(m_androidCamera, &AndroidCamera::mediaFinish, this, &PhoneCamera::onMediaFinish, Qt::DirectConnection);

	connect(driverHelper, &DriverHelper::driverReady, this, [=](QString path, PhoneType type) {
		if (m_phoneType == PhoneType::Android && type == PhoneType::Android)
			m_androidCamera->startTask(path);
	});

	switchPhoneType();
}

PhoneCamera::~PhoneCamera()
{
#ifdef DUMP_VIDEO
	m_videodump.close();
#endif

	delete m_androidCamera;
	delete m_iOSCamera;
}

void PhoneCamera::onMediaData(uint8_t *data, size_t size, int64_t timestamp, bool isVideo)
{
	if (isVideo) {
#ifdef DUMP_VIDEO
		m_videodump.write((char *)data, size);
#endif
		if (timestamp == ((int64_t)UINT64_C(0x8000000000000000))) {
			if (!ffmpeg_decode_valid(m_videoDecoder)) { // todo 单独的初始化流程
				if (ffmpeg_decode_init(m_videoDecoder, AV_CODEC_ID_H264, false, data, size) < 0) {
					blog(LOG_WARNING, "Could not initialize video decoder");
				}
			}
			return;
		}

		if (!ffmpeg_decode_valid(m_videoDecoder))
			return;

		bool got_output;
		long long ts = 0;
		bool success = ffmpeg_decode_video(m_videoDecoder, data, size, &ts, VIDEO_RANGE_DEFAULT, &frame, &got_output);
		if (!success) {
			blog(LOG_WARNING, "Error decoding video");
			return;
		}

		if (got_output) {
			frame.timestamp = timestamp;
			obs_source_output_video2(m_source, &frame);
		}
	}
}

void PhoneCamera::onMediaFinish()
{
	ffmpeg_decode_free(m_audioDecoder);
	ffmpeg_decode_free(m_videoDecoder);
	obs_source_output_video2(m_source, nullptr);
}

void PhoneCamera::switchPhoneType()
{
	obs_data_t *settings = obs_source_get_settings(m_source);
	obs_data_release(settings);

	PhoneType type = (PhoneType)obs_data_get_int(settings, PHONE_DEVICE_TYPE);
	if (type == m_phoneType)
		return;

	QString deviceId = obs_data_get_string(settings, DEVICE_ID);
	m_phoneType = (PhoneType)type;
	if (type == PhoneType::iOS) {
		m_iOSCamera->setCurrentDevice(deviceId);
	} else if (type == PhoneType::Android) {
		m_androidCamera->setCurrentDevice(deviceId);
	}

	driverHelper->checkDevices(m_phoneType);
}

QMap<QString, QPair<QString, uint32_t>> PhoneCamera::deviceList(int type)
{
	if (type == (int)PhoneType::iOS)
		return m_iOSDevices;
	else if (type == (int)PhoneType::Android)
		return driverHelper->androidDevices();

	return {};
}

static const char *GetPhoneCameraInputName(void *)
{
	return "xxq phone camera plugin";
}

static void UpdatePhoneCameraInput(void *data, obs_data_t *settings);

static void *CreatePhoneCameraInput(obs_data_t *settings, obs_source_t *source)
{
	PhoneCamera *cameraInput = nullptr;

	try {
		cameraInput = new PhoneCamera(settings, source);
	} catch (const char *error) {
		blog(LOG_ERROR, "Could not create device '%s': %s", obs_source_get_name(source), error);
	}

	return cameraInput;
}

static void DestroyPhoneCameraInput(void *data)
{
	delete reinterpret_cast<PhoneCamera *>(data);
}

static void DeactivatePhoneCameraInput(void *data)
{
	auto cameraInput = reinterpret_cast<PhoneCamera *>(data);
	//cameraInput->deactivate();
}

static void ActivatePhoneCameraInput(void *data)
{
	auto cameraInput = reinterpret_cast<PhoneCamera *>(data);
	//cameraInput->activate();
}

static bool UpdateClicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	auto settings = obs_source_get_settings(camera->source());
	obs_data_release(settings);
	auto devices = camera->deviceList(obs_data_get_int(settings, PHONE_DEVICE_TYPE));

	obs_property_t *dev_list = obs_properties_get(props, DEVICE_ID);
	obs_property_list_clear(dev_list);

	for (auto iter = devices.begin(); iter != devices.end(); iter++) {
		obs_property_list_add_string(dev_list, iter.value().first.toUtf8().data(), iter.key().toUtf8().data());
	}

	return true;
}

static bool TypeSelectionChanged(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	auto camera = obs_properties_get_param(props);
	UpdateClicked(props, p, camera);
	return true;
}

static obs_properties_t *GetPhoneCameraProperties(void *data)
{
	UNUSED_PARAMETER(data);

	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	obs_properties_t *ppts = obs_properties_create();
	obs_properties_set_param(ppts, camera, NULL);

	obs_property_t *pd = obs_properties_add_list(ppts, PHONE_DEVICE_TYPE, "device type", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(pd, "iOS", (int)PhoneType::iOS);
	obs_property_list_add_int(pd, "Android", (int)PhoneType::Android);
	obs_property_set_modified_callback(pd, TypeSelectionChanged);

	obs_property_t *p = obs_properties_add_list(ppts, DEVICE_ID, "iOS device list", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	UpdateClicked(ppts, p, camera);

	obs_properties_add_button(ppts, "update", "update device list", UpdateClicked);

	return ppts;
}

static void GetPhoneCameraDefaults(obs_data_t *settings) {}

static void SavePhoneCameraInput(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(settings);
}

static void UpdatePhoneCameraInput(void *data, obs_data_t *settings)
{
	Q_UNUSED(settings)
	PhoneCamera *input = reinterpret_cast<PhoneCamera *>(data);
	QMetaObject::invokeMethod(input, "switchPhoneType");
}

void RegisterPhoneCamera()
{
	obs_source_info info = {};
	info.id = "xxq-phone-camera-source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO;
	info.get_name = GetPhoneCameraInputName;

	info.create = CreatePhoneCameraInput;
	info.destroy = DestroyPhoneCameraInput;

	info.deactivate = DeactivatePhoneCameraInput;
	info.activate = ActivatePhoneCameraInput;

	info.get_defaults = GetPhoneCameraDefaults;
	info.get_properties = GetPhoneCameraProperties;
	info.save = SavePhoneCameraInput;
	info.update = UpdatePhoneCameraInput;
	obs_register_source(&info);
}
