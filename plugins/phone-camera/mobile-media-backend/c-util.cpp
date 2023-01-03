#include "c-util.h"
#include <qprocess.h>
#include <qmutex.h>
#include <qmap.h>

static QMutex g_mutex;
static uint32_t g_event_count = 1;
void usb_device_reset(const char *serial)
{
	QStringList p;
	p << serial;
	QProcess::startDetached("device-helper.exe", p);
}

void add_usb_device_change_event()
{
	QMutexLocker locker(&g_mutex);
	g_event_count += 1;
}

bool consume_usb_device_change_event()
{
	bool res = false;

	g_mutex.lock();

	if (g_event_count > 0) {
		g_event_count--;
		res = true;
	}

	g_mutex.unlock();

	return res;
}

static QMutex g_mediaCBMutex;
struct MediaCbInfo {
	media_data_cb cb1;
	device_lost cb2;
	void *cb_data;
};
QMap<QString, MediaCbInfo> g_mediaCBs;
void add_media_callback(const char *serial, media_data_cb cb, device_lost lost_cb, void *cb_data)
{
	QMutexLocker locker(&g_mediaCBMutex);
	g_mediaCBs.insert(serial, {cb, lost_cb, cb_data});
}

void remove_media_callback(const char *serial)
{
	QMutexLocker locker(&g_mediaCBMutex);
	g_mediaCBs.remove(serial);
}

void process_media_data(const char *serial, char *buf, int size)
{
	QMutexLocker locker(&g_mediaCBMutex);
	for (auto iter = g_mediaCBs.begin(); iter != g_mediaCBs.end(); iter++) {
		if (iter.key() == serial) {
			auto &info = iter.value();
			info.cb1(buf, size, info.cb_data);
			break;
		}
	}
}

void process_device_lost(const char *serial)
{
	QMutexLocker locker(&g_mediaCBMutex);
	for (auto iter = g_mediaCBs.begin(); iter != g_mediaCBs.end(); iter++) {
		if (iter.key() == serial) {
			auto &info = iter.value();
			info.cb2(info.cb_data);
			break;
		}
	}
}

bool media_callback_installed(const char *serial)
{
	QMutexLocker locker(&g_mediaCBMutex);
	return g_mediaCBs.contains(serial);
}
