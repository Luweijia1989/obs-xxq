#include "phone-camera.h"
#include <qfile.h>
#include <qhostaddress.h>
#include <qdebug.h>
#include <qtimer.h>
#include <qjsonarray.h>
#include <util/platform.h>

#define PHONE_DEVICE_TYPE "device_type"
#define DEVICE_ID "device_id"

//#define DUMP_VIDEO
PhoneCamera::PhoneCamera(obs_data_t *settings, obs_source_t *source) : m_source(source)
{
#ifdef DUMP_VIDEO
	m_videodump.setFileName("dump.h264");
	m_videodump.open(QFile::ReadWrite);
#endif // DUMP_VIDEO

	video_format_get_parameters(VIDEO_CS_601, VIDEO_RANGE_DEFAULT, frame.color_matrix, frame.color_range_min, frame.color_range_max);
	auto socket = new QTcpSocket(this);
	m_socketWrapper = new TcpSocketWrapper(socket);
	connect(m_socketWrapper, &TcpSocketWrapper::msgReceived, this, [=](QJsonObject msg) {
		int type = msg["type"].toInt();
		if ((MsgType)type == MsgType::DeviceListResult) {
			QJsonArray array = msg["data"].toArray();
			for (size_t i = 0; i < array.size(); i++) {
				auto obj = array.at(i).toObject();
				m_devices.insert(obj["id"].toString(), {obj["name"].toString(), obj["handle"].toString().toULong()});
			}
			m_blockEvent.quit();
		}
	});

	socket->connectToHost(QHostAddress::LocalHost, 51338);
	bool ret = socket->waitForConnected(100);
	qDebug() << "command socket connect result: " << (ret ? "success" : "fail");

	m_mediaDataServer = new MediaDataServer;
	connect(m_mediaDataServer, &MediaDataServer::mediaData, this, &PhoneCamera::onMediaData, Qt::DirectConnection);
	connect(m_mediaDataServer, &MediaDataServer::mediaVideoInfo, this, &PhoneCamera::onMediaVideoInfo, Qt::DirectConnection);
	m_mediaDataServer->start();
}

PhoneCamera::~PhoneCamera()
{
#ifdef DUMP_VIDEO
	m_videodump.close();
#endif
}

void PhoneCamera::onMediaVideoInfo(const media_video_info &info)
{
	if (info.video_extra_len == 0)
		return;

	ffmpeg_decode_free(m_videoDecoder);

	if (ffmpeg_decode_init(m_videoDecoder, AV_CODEC_ID_H264, false, info.video_extra, info.video_extra_len) < 0) {
		blog(LOG_WARNING, "Could not initialize video decoder");
	}
}

void PhoneCamera::onMediaData(uint8_t *data, size_t size, int64_t timestamp, bool isVideo)
{
	if (isVideo) {
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
	} else {
		obs_source_audio audio;
		audio.format = AUDIO_FORMAT_16BIT;
		audio.samples_per_sec = 48000;
		audio.speakers = SPEAKERS_STEREO;
		audio.frames = size / (2 * sizeof(short));
		audio.timestamp = os_gettime_ns();
		audio.data[0] = data;
		obs_source_output_audio(m_source, &audio);
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
	QString deviceId = obs_data_get_string(settings, DEVICE_ID);

	QJsonObject req;
	req["type"] = (int)MsgType::MediaTask;
	QJsonObject data;
	data["port"] = m_mediaDataServer->port();
	data["deviceType"] = (int)type;
	data["deviceId"] = deviceId;
	req["data"] = data;

	m_socketWrapper->sendMsg(QJsonDocument(req));
}

void PhoneCamera::taskEnd()
{
	delete m_mediaDataServer;
}

QMap<QString, QPair<QString, uint32_t>> PhoneCamera::deviceList(int type)
{
	m_devices.clear();

	QJsonObject req;
	if (type == (int)PhoneType::iOS)
		req["type"] = (int)MsgType::iOSDeviceList;
	else if (type == (int)PhoneType::Android)
		req["type"] = (int)MsgType::AndroidDeviceList;

	QTimer timer;
	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, &m_blockEvent, &QEventLoop::quit);
	m_socketWrapper->sendMsg(QJsonDocument(req));
	timer.start(100);
	m_blockEvent.exec(QEventLoop::ExcludeUserInputEvents);

	return m_devices;
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
		UpdatePhoneCameraInput(cameraInput, settings);
	} catch (const char *error) {
		blog(LOG_ERROR, "Could not create device '%s': %s", obs_source_get_name(source), error);
	}

	return cameraInput;
}

static void DestroyPhoneCameraInput(void *data)
{
	auto p = reinterpret_cast<PhoneCamera *>(data);
	p->taskEnd();
	delete p;
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

	obs_property_list_add_string(dev_list, "auto", "auto");
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

static void GetPhoneCameraDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, PHONE_DEVICE_TYPE, (int)PhoneType::iOS);
	obs_data_set_default_string(settings, DEVICE_ID, "auto");
}

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
