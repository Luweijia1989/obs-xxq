#pragma once

#include <Windows.h>
#include <QString>

#define USE_AUDIO

#define AOA_PROTOCOL_MIN 1
#define AOA_PROTOCOL_MAX 2

#define VID_GOOGLE 0x18D1
#define PID_AOA_ACC 0x2D00
#define PID_AOA_ACC_ADB 0x2D01
#define PID_AOA_AU 0x2D02
#define PID_AOA_AU_ADB 0x2D03
#define PID_AOA_ACC_AU 0x2D04
#define PID_AOA_ACC_AU_ADB 0x2D05

#define APPLE_VID 0x05AC
#define PID_RANGE_LOW 0x1290
#define PID_RANGE_MAX 0x12af

extern GUID USB_DEVICE_GUID;
extern GUID ADB_DEVICE_GUID;
extern QString GUID_HID;
extern QString GUID_KEYBOARD;
extern QString GUID_MOUSE;

extern char *vendor;
extern char *model; //根据这个model来打开手机的app
extern char *description;
extern char *version;
extern char *uri;
extern char *serial;

bool isAppleDevice(int vid, int pid);
int isAOADevice(int vid, int pid);
