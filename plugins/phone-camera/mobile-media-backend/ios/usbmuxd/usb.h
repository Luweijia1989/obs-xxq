/*
 * usb.h
 *
 * Copyright (C) 2009 Hector Martin <hector@marcansoft.com>
 * Copyright (C) 2009 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009 Martin Szulecki <opensuse@sukimashita.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef USB_H
#define USB_H

#include <stdint.h>
#include "utils.h"

#define INTERFACE_CLASS 255
#define INTERFACE_SUBCLASS 254
#define INTERFACE_PROTOCOL 2
#define INTERFACE_QUICKTIMECLASS 42

// libusb fragments packets larger than this (usbfs limitation)
// on input, this creates race conditions and other issues
#define USB_MRU 16384

// max transmission packet size
// libusb fragments these too, but doesn't send ZLPs so we're safe
// but we need to send a ZLP ourselves at the end (see usb-linux.c)
// we're using 3 * 16384 to optimize for the fragmentation
// this results in three URBs per full transfer, 32 USB packets each
// if there are ZLP issues this should make them show up easily too
#define USB_MTU (3 * 16384)

#define USB_PACKET_SIZE 512

#define VID_APPLE 0x5ac
#define PID_RANGE_LOW 0x1290
#define PID_RANGE_MAX 0x12af
#define PID_APPLE_T2_COPROCESSOR 0x8600

#define VID_GOOGLE 0x18D1
#define PID_AOA_ACC 0x2D00
#define PID_AOA_ACC_ADB 0x2D01
#define PID_AOA_ACC_AU 0x2D04
#define PID_AOA_ACC_AU_ADB 0x2D05

struct usb_device;

void usb_set_log_level(int level);
int usb_get_log_level(void);
int usb_initialize(void);
void usb_shutdown(void);
const char *usb_get_serial(struct usb_device *dev);
uint32_t usb_get_location(struct usb_device *dev);
uint16_t usb_get_pid(struct usb_device *dev);
uint64_t usb_get_speed(struct usb_device *dev);

#ifndef WIN32
void usb_get_fds(struct fdlist *list);
#endif

int usb_get_timeout(void);
int usb_send(struct usb_device *dev, const unsigned char *buf, int length, int for_mirror);
int usb_discover(void);
void usb_autodiscover(int enable);
int usb_process(void);
int usb_process_timeout(int msec);

void usb_enumerate_device();
void usb_stop_enumerate();

void usb_send_media_data(struct usb_device *dev, char *buf, int length);
void usb_camera_task_end(struct usb_device *dev);

#endif
