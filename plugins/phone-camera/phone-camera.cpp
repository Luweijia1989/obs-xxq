#include "phone-camera.h"
#include <qfile.h>
#include <util/platform.h>

#include "ios-camera.h"
#include "driver-helper.h"

#define IOS_DEVICE_LIST "iOS_device_list"
#define IOS_DEVICE_UDID "iOS_udid"

//#define DUMP_VIDEO

extern DriverHelper *driverHelper;

PhoneCamera::PhoneCamera(obs_data_t *settings, obs_source_t *source) : m_source(source)
{
#ifdef DUMP_VIDEO
	m_videodump.setFileName("dump.h264");
	m_videodump.open(QFile::ReadWrite);
#endif // DUMP_VIDEO

	video_format_get_parameters(VIDEO_CS_601, VIDEO_RANGE_DEFAULT, frame.color_matrix, frame.color_range_min, frame.color_range_max);

	m_iOSCamera = new iOSCamera(this);
	connect(m_iOSCamera, &iOSCamera::updateDeviceList, this, [=](QMap<QString, QPair<QString, uint32_t>> devices) { m_iOSDevices = devices; });
	connect(m_iOSCamera, &iOSCamera::mediaData, this,
		[this](uint8_t *data, size_t size, bool isVideo) {
			uint64_t curTs = os_gettime_ns();
			if (isVideo) {
#ifdef DUMP_VIDEO
				m_videodump.write((char *)data, size);
#endif
				if (!m_videoDecoder)
					m_videoDecoder = new Decoder();

				if (!ffmpeg_decode_valid(*m_videoDecoder)) {
					if (ffmpeg_decode_init(*m_videoDecoder, AV_CODEC_ID_H264, false) < 0) {
						blog(LOG_WARNING, "Could not initialize video decoder");
						return;
					}
				}

				long long ts = curTs;
				bool got_output;
				bool success = ffmpeg_decode_video(*m_videoDecoder, data, size, &ts, VIDEO_RANGE_DEFAULT, &frame, &got_output);
				if (!success) {
					blog(LOG_WARNING, "Error decoding video");
					return;
				}

				if (got_output) {
					frame.timestamp = curTs;
					obs_source_output_video2(m_source, &frame);
				}
			}
		},
		Qt::DirectConnection);
	connect(m_iOSCamera, &iOSCamera::mediaFinish, this,
		[this]() {
			if (m_audioDecoder) {
				delete m_audioDecoder;
				m_audioDecoder = nullptr;
			}
			if (m_videoDecoder) {
				delete m_videoDecoder;
				m_videoDecoder = nullptr;
			}
		},
		Qt::DirectConnection);

	int type = obs_data_get_int(settings, "phoneType");
	m_iOSCamera->setCurrentDevice(obs_data_get_string(settings, IOS_DEVICE_UDID));
	switchPhoneType(iOS);

	driverHelper->checkDevices();
}

PhoneCamera::~PhoneCamera()
{
#ifdef DUMP_VIDEO
	m_videodump.close();
#endif

	delete m_iOSCamera;
}

void PhoneCamera::switchPhoneType(int type)
{
	if (type == m_phoneType)
		return;

	m_phoneType = (PhoneType)type;

	m_iOSCamera->stop();
	if (type == iOS) {
		m_iOSCamera->start();
	} else if (type == Android) {
	}
}

const QMap<QString, QPair<QString, uint32_t>> &PhoneCamera::deviceList() const
{
	return m_iOSDevices;
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

static bool DeviceSelectionChanged(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{

	return false;
}

static bool UpdateClicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	auto devices = camera->deviceList();

	obs_property_t *dev_list = obs_properties_get(props, IOS_DEVICE_LIST);
	obs_property_list_clear(dev_list);

	for (auto iter = devices.begin(); iter != devices.end(); iter++) {
		obs_property_list_add_string(dev_list, iter.value().first.toUtf8().data(), iter.key().toUtf8().data());
	}

	return true;
}

static obs_properties_t *GetPhoneCameraProperties(void *data)
{
	UNUSED_PARAMETER(data);

	PhoneCamera *camera = reinterpret_cast<PhoneCamera *>(data);
	obs_properties_t *ppts = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(ppts, IOS_DEVICE_LIST, "iOS device list", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	UpdateClicked(ppts, p, camera);

	obs_property_set_modified_callback(p, DeviceSelectionChanged);
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
	PhoneCamera *input = reinterpret_cast<PhoneCamera *>(data);
	int type = obs_data_get_int(settings, "phoneType");
	QMetaObject::invokeMethod(input, "switchPhoneType", Q_ARG(int, 1));
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
