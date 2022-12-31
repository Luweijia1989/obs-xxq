#include "c-util.h"
#include <qprocess.h>
#include <qmutex.h>

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
