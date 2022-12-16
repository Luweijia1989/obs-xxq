#include "ios-screen-mirror-task-thread.h"
#include <qdebug.h>
#include <qelapsedtimer.h>
#include <util/circlebuf.h>
#include "IOS/usbmuxd/usbmuxd-proto.h"
#include <plist/plist.h>

iOSScreenMirrorTaskThread::iOSScreenMirrorTaskThread(QObject *parent) : TaskThread(parent)
{
	m_mirrorSocket = new QTcpSocket();
	//connect(this, &QThread::finished, m_mirrorSocket, &QTcpSocket::deleteLater);
	connect(m_mirrorSocket, &QTcpSocket::readyRead, m_mirrorSocket, [=]() { QByteArray data = m_mirrorSocket->readAll(); });
	connect(m_mirrorSocket, &QTcpSocket::destroyed, m_mirrorSocket, [=]() { qDebug() << "ffffffffffffff"; });
	connect(m_mirrorSocket, &QTcpSocket::connected, m_mirrorSocket, [=]() {
		sendCmd(true);
	});
	connect(m_mirrorSocket, &QTcpSocket::disconnected, this, [=](){
		m_eventLoop.quit();
	});
	m_mirrorSocket->moveToThread(this);
}

void iOSScreenMirrorTaskThread::sendCmd(bool isStart)
{
	QString tid = m_udid.replace('-', "").toUpper();

	plist_t dict = plist_new_dict();
	plist_dict_set_item(dict, "CmdType", plist_new_string(isStart ? "Start" : "Stop"));
	plist_dict_set_item(dict, "DeviceId", plist_new_string(tid.toUtf8().data()));

	int res = -1;
	char *xml = NULL;
	uint32_t xmlsize = 0;
	plist_to_xml(dict, &xml, &xmlsize);
	if (xml) {
		struct usbmuxd_header hdr;
		auto size = sizeof(hdr) + xmlsize;
		hdr.version = 1;
		hdr.length = size;
		hdr.message = MESSAGE_CUSTOM;
		hdr.tag = 1;
		auto sendBuffer = (uint8_t *)malloc(size);
		memcpy(sendBuffer, &hdr, sizeof(hdr));
		memcpy(sendBuffer + sizeof(hdr), xml, xmlsize);
		m_mirrorSocket->write((char *)sendBuffer, size);
		m_mirrorSocket->waitForBytesWritten(1000);
		free(sendBuffer);
		free(xml);
	}
	plist_free(dict);

	if (!isStart)
		m_mirrorSocket->disconnectFromHost();
}

void iOSScreenMirrorTaskThread::startTask(QString udid, uint32_t deviceHandle)
{
	TaskThread::startTask(udid, deviceHandle);

	QMetaObject::invokeMethod(m_mirrorSocket, [=]() { m_mirrorSocket->connectToHost(QHostAddress::LocalHost, USBMUXD_SOCKET_PORT); });
}

void iOSScreenMirrorTaskThread::stopTask()
{
	if (!isRunning())
		return;

	QMetaObject::invokeMethod(m_mirrorSocket, [=]() {
		sendCmd(false);
	});

	m_eventLoop.exec();

	quit();
	TaskThread::stopTask();

	if (!m_inMirror)
		return;

	//{
	//	uint8_t *d1;
	//	size_t d1_len;
	//	NewAsynHPA0(mp->deviceAudioClockRef, &d1, &d1_len);
	//	writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
	//	free(d1);
	//}

	//{
	//	uint8_t *d1;
	//	size_t d1_len;
	//	NewAsynHPD0(&d1, &d1_len);
	//	writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
	//	free(d1);
	//}

	//QTimer timer;
	//timer.setSingleShot(true);
	//connect(&timer, &QTimer::timeout, &mp->quitBlockEvent, &QEventLoop::quit);
	//timer.start(1000);
	//mp->quitBlockEvent.exec();

	//{
	//	uint8_t *d1;
	//	size_t d1_len;
	//	NewAsynHPD0(&d1, &d1_len);
	//	writeUBSData(ep_out_fa, (char *)d1, d1_len, 1000);
	//	free(d1);
	//	qDebug("OK. Ready to release USB Device.");
	//}

	//usb_release_interface(m_handle, m_interface_fa);
	//usb_control_msg(m_handle, 0x40, 0x52, 0x00, 0x00, NULL, 0, 1000 /* LIBUSB_DEFAULT_TIMEOUT */);
	//usb_set_configuration(m_handle, 4);

	//TaskThread::stopTask();

	//usb_close(m_handle);
	//m_handle = nullptr;

	//delete mp;
}
