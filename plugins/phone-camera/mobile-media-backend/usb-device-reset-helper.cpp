#include "usb-device-reset-helper.h"
#include <qprocess.h>

void usb_device_reset(const char *serial)
{
	QStringList p;
	p << serial;
	QProcess::startDetached("device-helper.exe", p);
}
