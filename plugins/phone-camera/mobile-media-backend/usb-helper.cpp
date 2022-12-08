#include "usb-helper.h"
#include <winusb.h>
#include <qthread.h>
#include <libusb-1.0/libusb.h>

GUID USB_DEVICE_GUID = {0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
GUID ADB_DEVICE_GUID = {0xf72fe0d4, 0xcbcb, 0x407d, {0x88, 0x14, 0x9e, 0xd6, 0x73, 0xd0, 0xdd, 0x6b}};

char *vendor = "YPP";
char *model = "XxqProjectionApp"; //根据这个model来打开手机的app
char *description = u8"鱼耳直播助手";
char *version = "1.0";
char *uri = "https://www.yuerzhibo.com";
char *serial = "0000000012345678";

QString GUID_HID = "745A17A0-74D3-11D0-B6FE-00A0C90F57DA";
QString GUID_KEYBOARD = "4d36e96b-e325-11ce-bfc1-08002be10318";
QString GUID_MOUSE = "4d36e96f-e325-11ce-bfc1-08002be10318";

static bool isAndroidADBDevice(int vid, int pid)
{
	return vid == VID_GOOGLE && (pid == PID_AOA_ACC_AU_ADB || pid == PID_AOA_AU_ADB || pid == PID_AOA_ACC_AU_ADB);
}

bool isAppleDevice(int vid, int pid)
{
	return vid == APPLE_VID && pid > PID_RANGE_LOW && pid < PID_RANGE_MAX;
}

int isAOADevice(int vid, int pid)
{
	if (vid == VID_GOOGLE) {
		switch (pid) {
		case PID_AOA_ACC:
		case PID_AOA_ACC_ADB:
		case PID_AOA_ACC_AU:
		case PID_AOA_ACC_AU_ADB:
			return 1;
		case PID_AOA_AU:
		case PID_AOA_AU_ADB:
			qDebug("device is audio-only\n");
			break;
		default:
			break;
		}
	}

	return 0;
}

QString serialNumber(QString path)
{
	path = path.left(path.lastIndexOf('#'));
	return path.mid(path.lastIndexOf('#') + 1);
}
