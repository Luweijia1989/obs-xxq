#include "application.h"
#include <qmetatype.h>
#include <qtcpsocket.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>

#include "media-source.h"
#include <libimobiledevice/lockdown.h>

extern "C" {
extern int usbmuxd_process();
extern int should_exit;
}

QSet<QString> installingDevices;
QSet<QString> runningDevices;
QSet<QString> iOSDevices;
QSet<QString> AndroidDevices;

static QJsonArray deviceList2Json(const QMap<QString, QPair<QString, uint32_t>> devices)
{
	QJsonArray array;
	for (auto iter = devices.begin(); iter != devices.end(); iter++) {
		QJsonObject obj;
		obj["id"] = iter.key();
		obj["name"] = iter.value().first;
		obj["handle"] = QString::number(iter.value().second);
		array.append(obj);
	}

	return array;
}

static QString getDeviceName(QString udid)
{
	QString result;

	idevice_t lockdown_device = NULL;
	idevice_new_with_options(&lockdown_device, udid.toUtf8().data(), IDEVICE_LOOKUP_USBMUX);

	if (!lockdown_device) {
		return result;
	}

	lockdownd_client_t lockdown = NULL;

	if (LOCKDOWN_E_SUCCESS != lockdownd_client_new(lockdown_device, &lockdown, "usbmuxd")) {
		idevice_free(lockdown_device);
		return result;
	}

	char *lockdown_device_name = NULL;

	if ((LOCKDOWN_E_SUCCESS != lockdownd_get_device_name(lockdown, &lockdown_device_name)) || !lockdown_device_name) {
		idevice_free(lockdown_device);
		lockdownd_client_free(lockdown);
		return result;
	}

	idevice_free(lockdown_device);
	lockdownd_client_free(lockdown);

	result = lockdown_device_name;
	free(lockdown_device_name);

	return result;
}

void Application::usbmuxdDeviceEvent(const usbmuxd_event_t *event, void *user_data)
{
	Application *app = (Application *)user_data;
	if (event->event == UE_DEVICE_ADD) {
		if (event->device.conn_type != CONNECTION_TYPE_USB)
			return;

		char *buf = nullptr;
		uint32_t size = 0;
		if (usbmuxd_read_pair_record(event->device.udid, &buf, &size) == 0) {
			free(buf);

			QMutexLocker locker(&app->m_devicesMutex);
			app->m_iOSDevices.insert(event->device.udid, {getDeviceName(event->device.udid), event->device.handle});
		}
	} else if (event->event == UE_DEVICE_PAIRED) {
		QMutexLocker locker(&app->m_devicesMutex);
		app->m_iOSDevices.insert(event->device.udid, {getDeviceName(event->device.udid), event->device.handle});
	} else if (event->event == UE_DEVICE_REMOVE) {
		QMutexLocker locker(&app->m_devicesMutex);
		app->m_iOSDevices.remove(event->device.udid);
	}
}

Application::Application(int &argc, char **argv) : QApplication(argc, argv)
{
	qRegisterMetaType<QMap<QString, QString>>("QMap<QString, QPair<QString, uint32_t>>");
	qRegisterMetaType<int64_t>("int64_t");

	usbmuxd_subscribe(usbmuxdDeviceEvent, this);
	usbmuxd_th = std::thread([] { usbmuxd_process(); });

	connect(&m_controlServer, &QTcpServer::newConnection, this, &Application::onNewConnection);
	m_controlServer.listen(QHostAddress::LocalHost, 51338);

	//QTimer::singleShot(100, this, [=](){
	//	QJsonObject data;
	//	data["port"] = 123;
	//	data["deviceType"] = (int)PhoneType::iOS;
	//	data["deviceId"] = "auto";
	//	onMediaTask(data);
	//});
}

Application::~Application()
{
	usbmuxd_unsubscribe();

	should_exit = 1;
	if (usbmuxd_th.joinable())
		usbmuxd_th.join();
}

void Application::onNewConnection()
{
	if (!m_controlServer.hasPendingConnections())
		return;

	auto client = m_controlServer.nextPendingConnection();
	TcpSocketWrapper *wrapper = new TcpSocketWrapper(client);
	connect(wrapper, &TcpSocketWrapper::msgReceived, this, [=](QJsonObject msg) {
		int type = msg["type"].toInt();
		if (type == (int)MsgType::iOSDeviceList || type == (int)MsgType::AndroidDeviceList)
			sendDeviceList(wrapper, type);
		else if (type == (int)MsgType::MediaTask) {
			onMediaTaskStart(msg["data"].toObject());
		}
	});
}

void Application::sendDeviceList(TcpSocketWrapper *wrapper, int type)
{
	QMap<QString, QPair<QString, uint32_t>> devices;
	if (type == (int)MsgType::iOSDeviceList) {
		QMutexLocker locker(&m_devicesMutex);
		devices = m_iOSDevices;
	} else if (type == (int)MsgType::AndroidDeviceList) {
		devices = m_driverHelper.androidDevices();
	}

	auto array = deviceList2Json(devices);
	QJsonObject obj;
	obj["type"] = (int)MsgType::DeviceListResult;
	obj["data"] = array;
	QJsonDocument doc(obj);
	wrapper->sendMsg(doc);
}

void Application::onMediaTaskStart(const QJsonObject &req)
{
	int tcpPort = req["port"].toInt();
	PhoneType type = (PhoneType)req["deviceType"].toInt();
	QString id = req["deviceId"].toString();

	MediaSource *source = nullptr;
	if (m_mediaSources.contains(tcpPort))
		source = m_mediaSources[tcpPort];
	else {
		source = new MediaSource();
		m_mediaSources.insert(tcpPort, source);
		connect(source, &MediaSource::destroyed, this, [=]() {
			m_mediaSources.remove(tcpPort);
		});
		source->connectToHost(tcpPort);
	}
	source->setCurrentDevice(type, id);

	qDebug() << "onMediaTaskStart, port: " << tcpPort;
}
