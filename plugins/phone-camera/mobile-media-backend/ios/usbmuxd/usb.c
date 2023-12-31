/*
 * usb.c
 *
 * Copyright (C) 2009 Hector Martin <hector@marcansoft.com>
 * Copyright (C) 2009 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2009-2020 Martin Szulecki <martin.szulecki@libimobiledevice.org>
 * Copyright (C) 2014 Mikkel Kamstrup Erlandsen <mikkel.kamstrup@xamarin.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef WIN32
#include "libusb-1.0/libusb.h"
#include "usb_win32.h"
#else
#include <libusb.h>
#endif

#include <pthread.h>

#include "usb.h"
#include "log.h"
#include "device.h"
#include "utils.h"
#include "media_process.h"
#include "../../c-util.h"

//change device scan to another thread, usb_device_add param from libusb_device to libusb_device_handle.
//disable hotplug detect

//#if (defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000102)) || (defined(LIBUSBX_API_VERSION) && (LIBUSBX_API_VERSION >= 0x01000102))
//#define HAVE_LIBUSB_HOTPLUG_API 1
//#endif

#if !defined(LIBUSB_API_VERSION) || LIBUSB_API_VERSION < 0x01000103
#include <stdio.h>

// On Linux, we compile using Ubuntu Precise, and libusb_strerror is not available on the version of
// libusb-1.0 that ships with Precise. In that case, we just convert the error code to a number
const char *libusb_strerror(int errcode)
{
	char str[3];
	snprintf(str, 3, "%d", errcode);
	return str;
}
#endif

// interval for device connection/disconnection polling, in milliseconds
// we need this because there is currently no asynchronous device discovery mechanism in libusb
#define DEVICE_POLL_TIME 100

// Number of parallel bulk transfers we have running for reading data from the device.
// Older versions of usbmuxd kept only 1, which leads to a mostly dormant USB port.
// 3 seems to be an all round sensible number - giving better read perf than
// Apples usbmuxd, at least.
#define NUM_RX_LOOPS 3

int libusb_verbose = 0;

enum mirror_status {
	not_in_mirror,
	in_mirror,
};

struct usb_device {
	libusb_device_handle *dev;
	uint8_t bus, address;
	char serial[256];
	char serial_usb[256];
	int alive;
	uint8_t interface, ep_in, ep_out;
	uint8_t interface_fa, ep_in_fa, ep_out_fa;
	struct collection rx_xfers;
	struct collection rx_xfers_for_mirror;
	struct collection tx_xfers;
	int wMaxPacketSize;
	uint64_t speed;
	struct libusb_device_descriptor devdesc;
	void *mirror_ctx;
	enum mirror_status status;
	int first_send;
	int is_iOS;
};

static struct collection device_list;
static struct collection device_handle_list;
static struct collection device_opened_handle_list;

static struct timeval next_dev_poll_time;

static int devlist_failures;
static int device_polling;
static int device_hotplug = 1;

static pthread_mutex_t devices_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
static pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER;

#define DELTA_EPOCH_IN_MICROSECS 11644473600000000Ui64
struct timezone {
	int tz_minuteswest; /* minutes W of Greenwich */
	int tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;

	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		tmpres /= 10; /*convert into microseconds*/
		/*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

int usb_is_aoa_device(uint16_t vid, uint16_t pid)
{
	if (vid == VID_GOOGLE && (pid == PID_AOA_ACC || pid == PID_AOA_ACC_ADB || pid == PID_AOA_ACC_AU || pid == PID_AOA_ACC_AU_ADB))
		return 1;
	else
		return 0;
}

void usb_set_log_level(int level)
{
	libusb_verbose = level;
}

int usb_get_log_level(void)
{
	return libusb_verbose;
}

static void usb_disconnect(struct usb_device *dev)
{
	if (!dev->dev) {
		return;
	}

	// kill the rx xfer and tx xfers and try to make sure the callbacks
	// get called before we free the device
	FOREACH(struct libusb_transfer * xfer, &dev->rx_xfers)
	{
		usbmuxd_log(LL_DEBUG, "usb_disconnect: cancelling RX xfer %p", xfer);
		libusb_cancel_transfer(xfer);
	}
	ENDFOREACH

	FOREACH(struct libusb_transfer * xfer, &dev->rx_xfers_for_mirror)
	{
		usbmuxd_log(LL_DEBUG, "usb_disconnect: cancelling RX xfer for mirror %p", xfer);
		libusb_cancel_transfer(xfer);
	}
	ENDFOREACH

	FOREACH(struct libusb_transfer * xfer, &dev->tx_xfers)
	{
		usbmuxd_log(LL_DEBUG, "usb_disconnect: cancelling TX xfer %p", xfer);
		libusb_cancel_transfer(xfer);
	}
	ENDFOREACH

	// Busy-wait until all xfers are closed
	while (collection_count(&dev->rx_xfers) || collection_count(&dev->tx_xfers)) {
		struct timeval tv;
		int res;

		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		if ((res = libusb_handle_events_timeout(NULL, &tv)) < 0) {
			usbmuxd_log(LL_ERROR, "libusb_handle_events_timeout for usb_disconnect failed: %s", libusb_error_name(res));
			break;
		}
	}

	while (collection_count(&dev->rx_xfers_for_mirror)) {
		struct timeval tv;
		int res;

		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		if ((res = libusb_handle_events_timeout(NULL, &tv)) < 0) {
			usbmuxd_log(LL_ERROR, "libusb_handle_events_timeout for usb_disconnect failed: %s", libusb_error_name(res));
			break;
		}
	}

	collection_free(&dev->tx_xfers);
	collection_free(&dev->rx_xfers);
	collection_free(&dev->rx_xfers_for_mirror);
	libusb_release_interface(dev->dev, dev->interface);

	pthread_mutex_lock(&devices_lock);
	libusb_close(dev->dev);
	collection_remove(&device_opened_handle_list, dev->dev);
	pthread_mutex_unlock(&devices_lock);

	add_usb_device_change_event();

	dev->dev = NULL;
	collection_remove(&device_list, dev);
	free(dev);
}

static void reap_dead_devices(void)
{
	FOREACH(struct usb_device * usbdev, &device_list)
	{
		if (!usbdev->alive) {
			if (usbdev->status == in_mirror) {
				send_state(usbdev, 1);
				process_device_lost(usbdev->serial_usb);
			}
			device_remove(usbdev);
			usb_disconnect(usbdev);
		}
	}
	ENDFOREACH
}

// Callback from write operation
static void LIBUSB_CALL tx_callback(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;
	//usbmuxd_log(LL_SPEW, "TX callback dev %d-%d len %d -> %d status %d", dev->bus, dev->address, xfer->length, xfer->actual_length, xfer->status);
	if (xfer->status != LIBUSB_TRANSFER_COMPLETED) {
		switch (xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: //shut up compiler
		case LIBUSB_TRANSFER_ERROR:
			// funny, this happens when we disconnect the device while waiting for a transfer, sometimes
			usbmuxd_log(LL_INFO, "Device %d-%d TX aborted due to error or disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			usbmuxd_log(LL_ERROR, "TX transfer timed out for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			usbmuxd_log(LL_DEBUG, "Device %d-%d TX transfer cancelled", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_STALL:
			usbmuxd_log(LL_ERROR, "TX transfer stalled for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			// other times, this happens, and also even when we abort the transfer after device removal
			usbmuxd_log(LL_INFO, "Device %d-%d TX aborted due to disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			usbmuxd_log(LL_ERROR, "TX transfer overflow for device %d-%d", dev->bus, dev->address);
			break;
		// and nothing happens (this never gets called) if the device is freed after a disconnect! (bad)
		default:
			// this should never be reached.
			break;
		}
		// we can't usb_disconnect here due to a deadlock, so instead mark it as dead and reap it after processing events
		// we'll do device_remove there too
		dev->alive = 0;
	}
	if (xfer->buffer)
		free(xfer->buffer);
	collection_remove(&dev->tx_xfers, xfer);
	libusb_free_transfer(xfer);
}

int usb_send(struct usb_device *dev, const unsigned char *buf, int length, int for_mirror)
{
	int res;
	struct libusb_transfer *xfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(xfer, dev->dev, for_mirror == 0 ? dev->ep_out : dev->ep_out_fa, (void *)buf, length, tx_callback, dev, 0);
	if ((res = libusb_submit_transfer(xfer)) < 0) {
		usbmuxd_log(LL_ERROR, "Failed to submit TX transfer %p len %d to device %d-%d: %s", buf, length, dev->bus, dev->address,
			    libusb_error_name(res));
		libusb_free_transfer(xfer);
		return res;
	}
	collection_add(&dev->tx_xfers, xfer);
	if (length % dev->wMaxPacketSize == 0) {
		usbmuxd_log(LL_DEBUG, "Send ZLP");
		// Send Zero Length Packet
		xfer = libusb_alloc_transfer(0);
		void *buffer = malloc(1);
		libusb_fill_bulk_transfer(xfer, dev->dev, for_mirror == 0 ? dev->ep_out : dev->ep_out_fa, buffer, 0, tx_callback, dev, 0);
		if ((res = libusb_submit_transfer(xfer)) < 0) {
			usbmuxd_log(LL_ERROR, "Failed to submit TX ZLP transfer to device %d-%d: %s", dev->bus, dev->address, libusb_error_name(res));
			libusb_free_transfer(xfer);
			return res;
		}
		collection_add(&dev->tx_xfers, xfer);
	}
	return 0;
}

static void clear_mirror_transfer(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;

	if (dev->is_iOS)
		destory_mirror_info(dev->mirror_ctx);
	else
		destroy_android_mirror_info(dev->mirror_ctx);

	dev->mirror_ctx = NULL;

	free(xfer->buffer);
	collection_remove(&dev->rx_xfers_for_mirror, xfer);
	libusb_free_transfer(xfer);

	// we can't usb_disconnect here due to a deadlock, so instead mark it as dead and reap it after processing events
	// we'll do device_remove there too
	dev->alive = 0;
}

static void LIBUSB_CALL rx_callback_for_mirror(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;
	if (!dev->mirror_ctx) {
		if (dev->is_iOS)
			dev->mirror_ctx = create_mirror_info(dev);
		else
			dev->mirror_ctx = create_android_mirror_info(dev);
	}

	//usbmuxd_log(LL_SPEW, "RX callback for mirror dev %d-%d len %d status %d", dev->bus, dev->address, xfer->actual_length, xfer->status);
	if (xfer->status == LIBUSB_TRANSFER_COMPLETED && xfer->actual_length != 0) {
		if (dev->is_iOS)
			onMirrorData(dev->mirror_ctx, xfer->buffer, xfer->actual_length);
		else
			on_android_mirror_data(dev->mirror_ctx, xfer->buffer, xfer->actual_length);

		int res = libusb_submit_transfer(xfer);
		if (res != 0)
			clear_mirror_transfer(xfer);
	} else {
		switch (xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: //shut up compiler
		case LIBUSB_TRANSFER_ERROR:
			// funny, this happens when we disconnect the device while waiting for a transfer, sometimes
			usbmuxd_log(LL_INFO, "Device %d-%d RX aborted due to error or disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			usbmuxd_log(LL_ERROR, "RX transfer timed out for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			usbmuxd_log(LL_DEBUG, "Device %d-%d RX transfer cancelled", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_STALL:
			usbmuxd_log(LL_ERROR, "RX transfer stalled for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			// other times, this happens, and also even when we abort the transfer after device removal
			usbmuxd_log(LL_INFO, "Device %d-%d RX aborted due to disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			usbmuxd_log(LL_ERROR, "RX transfer overflow for device %d-%d", dev->bus, dev->address);
			break;
		// and nothing happens (this never gets called) if the device is freed after a disconnect! (bad)
		default:
			// this should never be reached.
			break;
		}

		clear_mirror_transfer(xfer);
	}
}

// Start a read-callback loop for this device
static int start_rx_loop_for_mirror(struct usb_device *dev)
{
	int res;
	void *buf;
	struct libusb_transfer *xfer = libusb_alloc_transfer(0);
	buf = malloc(USB_MRU);
	libusb_fill_bulk_transfer(xfer, dev->dev, dev->is_iOS ? dev->ep_in_fa : dev->ep_in, buf, USB_MRU, rx_callback_for_mirror, dev, 0);
	if ((res = libusb_submit_transfer(xfer)) != 0) {
		usbmuxd_log(LL_ERROR, "Failed to submit RX transfer to device for mirror %d-%d: %s", dev->bus, dev->address, libusb_error_name(res));
		libusb_free_transfer(xfer);
		return res;
	}

	collection_add(&dev->rx_xfers_for_mirror, xfer);

	return 0;
}

static void clear_transfer(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;

	free(xfer->buffer);
	collection_remove(&dev->rx_xfers, xfer);
	libusb_free_transfer(xfer);

	// we can't usb_disconnect here due to a deadlock, so instead mark it as dead and reap it after processing events
	// we'll do device_remove there too
	dev->alive = 0;
}

// Callback from read operation
// Under normal operation this issues a new read transfer request immediately,
// doing a kind of read-callback loop
static void LIBUSB_CALL rx_callback(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;
	//usbmuxd_log(LL_SPEW, "RX callback dev %d-%d len %d status %d", dev->bus, dev->address, xfer->actual_length, xfer->status);
	if (xfer->status == LIBUSB_TRANSFER_COMPLETED && xfer->actual_length != 0) {
		device_data_input(dev, xfer->buffer, xfer->actual_length);
		int res = libusb_submit_transfer(xfer);
		if (res != 0)
			clear_transfer(xfer);
	} else {
		switch (xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: //shut up compiler
		case LIBUSB_TRANSFER_ERROR:
			// funny, this happens when we disconnect the device while waiting for a transfer, sometimes
			usbmuxd_log(LL_INFO, "Device %d-%d RX aborted due to error or disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			usbmuxd_log(LL_ERROR, "RX transfer timed out for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			usbmuxd_log(LL_DEBUG, "Device %d-%d RX transfer cancelled", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_STALL:
			usbmuxd_log(LL_ERROR, "RX transfer stalled for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			// other times, this happens, and also even when we abort the transfer after device removal
			usbmuxd_log(LL_INFO, "Device %d-%d RX aborted due to disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			usbmuxd_log(LL_ERROR, "RX transfer overflow for device %d-%d", dev->bus, dev->address);
			break;
		// and nothing happens (this never gets called) if the device is freed after a disconnect! (bad)
		default:
			// this should never be reached.
			break;
		}

		clear_transfer(xfer);
	}
}

// Start a read-callback loop for this device
static int start_rx_loop(struct usb_device *dev)
{
	int res;
	void *buf;
	struct libusb_transfer *xfer = libusb_alloc_transfer(0);
	buf = malloc(USB_MRU);
	libusb_fill_bulk_transfer(xfer, dev->dev, dev->ep_in, buf, USB_MRU, rx_callback, dev, 0);
	if ((res = libusb_submit_transfer(xfer)) != 0) {
		usbmuxd_log(LL_ERROR, "Failed to submit RX transfer to device %d-%d: %s", dev->bus, dev->address, libusb_error_name(res));
		libusb_free_transfer(xfer);
		return res;
	}

	collection_add(&dev->rx_xfers, xfer);

	return 0;
}

static void LIBUSB_CALL get_serial_callback(struct libusb_transfer *transfer)
{
	unsigned int di, si;
	struct usb_device *usbdev = transfer->user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		usbmuxd_log(LL_ERROR, "Failed to request serial for device %d-%d (%i)", usbdev->bus, usbdev->address, transfer->status);
		libusb_free_transfer(transfer);
		return;
	}

	/* De-unicode, taken from libusb */
	unsigned char *data = libusb_control_transfer_get_data(transfer);
	for (di = 0, si = 2; si < data[0] && di < sizeof(usbdev->serial) - 1; si += 2) {
		if ((data[si] & 0x80) || (data[si + 1])) /* non-ASCII */
			usbdev->serial[di++] = '?';
		else if (data[si] == '\0')
			break;
		else
			usbdev->serial[di++] = data[si];
	}
	usbdev->serial[di] = '\0';

	usbmuxd_log(LL_INFO, "Got serial '%s' for device %d-%d", usbdev->serial, usbdev->bus, usbdev->address);

	libusb_free_transfer(transfer);

	/* new style UDID: add hyphen between first 8 and following 16 digits */
	if (di == 24) {
		memmove(&usbdev->serial[9], &usbdev->serial[8], 16);
		usbdev->serial[8] = '-';
		usbdev->serial[di + 1] = '\0';
	}
	usbmuxd_log(LL_INFO, "Got serial '%s' for device %d-%d", usbdev->serial, usbdev->bus, usbdev->address);
	/* Finish setup now */
	if (device_add(usbdev) < 0) {
		usb_disconnect(usbdev);
		return;
	}

	// Spin up NUM_RX_LOOPS parallel usb data retrieval loops
	// Old usbmuxds used only 1 rx loop, but that leaves the
	// USB port sleeping most of the time
	int rx_loops = NUM_RX_LOOPS;
	for (rx_loops = NUM_RX_LOOPS; rx_loops > 0; rx_loops--) {
		if (start_rx_loop(usbdev) < 0) {
			usbmuxd_log(LL_WARNING, "Failed to start RX loop number %d", NUM_RX_LOOPS - rx_loops);
			break;
		}
	}

	// Ensure we have at least 1 RX loop going
	if (rx_loops == NUM_RX_LOOPS) {
		usbmuxd_log(LL_FATAL, "Failed to start any RX loop for device %d-%d", usbdev->bus, usbdev->address);
		device_remove(usbdev);
		usb_disconnect(usbdev);
		return;
	} else if (rx_loops > 0) {
		usbmuxd_log(LL_WARNING,
			    "Failed to start all %d RX loops. Going on with %d loops. "
			    "This may have negative impact on device read speed.",
			    NUM_RX_LOOPS, NUM_RX_LOOPS - rx_loops);
	} else {
		usbmuxd_log(LL_DEBUG, "All %d RX loops started successfully", NUM_RX_LOOPS);
	}
}

static void LIBUSB_CALL get_langid_callback(struct libusb_transfer *transfer)
{
	int res;
	struct usb_device *usbdev = transfer->user_data;

	transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		usbmuxd_log(LL_ERROR, "Failed to request lang ID for device %d-%d (%i)", usbdev->bus, usbdev->address, transfer->status);
		libusb_free_transfer(transfer);
		return;
	}

	unsigned char *data = libusb_control_transfer_get_data(transfer);
	uint16_t langid = (uint16_t)(data[2] | (data[3] << 8));
	usbmuxd_log(LL_INFO, "Got lang ID %u for device %d-%d", langid, usbdev->bus, usbdev->address);

	/* re-use the same transfer */
	libusb_fill_control_setup(transfer->buffer, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR,
				  (uint16_t)((LIBUSB_DT_STRING << 8) | usbdev->devdesc.iSerialNumber), langid, 1024 + LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_transfer(transfer, usbdev->dev, transfer->buffer, get_serial_callback, usbdev, 1000);

	if ((res = libusb_submit_transfer(transfer)) < 0) {
		usbmuxd_log(LL_ERROR, "Could not request transfer for device %d-%d: %s", usbdev->bus, usbdev->address, libusb_error_name(res));
		libusb_free_transfer(transfer);
	}
}

void usb_send_media_data(struct usb_device *dev, char *buf, int length)
{
	process_media_data(dev->serial_usb, buf, length);
}

static void LIBUSB_CALL android_tx_callback(struct libusb_transfer *xfer)
{
	struct usb_device *dev = xfer->user_data;
	if (xfer->status != LIBUSB_TRANSFER_COMPLETED) {
		switch (xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: //shut up compiler
		case LIBUSB_TRANSFER_ERROR:
			// funny, this happens when we disconnect the device while waiting for a transfer, sometimes
			usbmuxd_log(LL_INFO, "Device %d-%d TX aborted due to error or disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			usbmuxd_log(LL_ERROR, "TX transfer timed out for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			usbmuxd_log(LL_DEBUG, "Device %d-%d TX transfer cancelled", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_STALL:
			usbmuxd_log(LL_ERROR, "TX transfer stalled for device %d-%d", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			// other times, this happens, and also even when we abort the transfer after device removal
			usbmuxd_log(LL_INFO, "Device %d-%d TX aborted due to disconnect", dev->bus, dev->address);
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			usbmuxd_log(LL_ERROR, "TX transfer overflow for device %d-%d", dev->bus, dev->address);
			break;
		// and nothing happens (this never gets called) if the device is freed after a disconnect! (bad)
		default:
			// this should never be reached.
			break;
		}
		// we can't usb_disconnect here due to a deadlock, so instead mark it as dead and reap it after processing events
		// we'll do device_remove there too
		dev->alive = 0;
	} else {
		if (!dev->first_send) {
			dev->first_send = 1;
			start_rx_loop_for_mirror(dev);
		}
	}
	if (xfer->buffer)
		free(xfer->buffer);
	collection_remove(&dev->tx_xfers, xfer);
	libusb_free_transfer(xfer);
}

static int usb_android_send(struct usb_device *usbdev, void *data, int size)
{
	int res;
	struct libusb_transfer *xfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(xfer, usbdev->dev, usbdev->ep_out, data, size, android_tx_callback, usbdev, 0);
	if ((res = libusb_submit_transfer(xfer)) < 0) {
		libusb_free_transfer(xfer);
		return res;
	}
	collection_add(&usbdev->tx_xfers, xfer);
	return 0;
}

static int usb_android_task_start(struct usb_device *usbdev)
{
	usbdev->status = in_mirror;
	usbdev->first_send = 0;

	uint8_t *fc = malloc(1);
	fc[0] = 1;
	if (usb_android_send(usbdev, fc, 1) < 0)
		free(fc);

	return 0;
}

// -1 error, 0 not android device, 1 sucess
static int usb_android_device_add(libusb_device_handle *handle)
{
	libusb_device *dev = libusb_get_device(handle);
	struct libusb_device_descriptor desc;
	if (libusb_get_device_descriptor(dev, &desc) != LIBUSB_SUCCESS)
		return -1;

	if (!usb_is_aoa_device(desc.idVendor, desc.idProduct))
		return 0;

	uint8_t bus = libusb_get_bus_number(dev);
	uint8_t address = libusb_get_device_address(dev);
	int found = 0;
	FOREACH(struct usb_device * usbdev, &device_list)
	{
		if (usbdev->bus == bus && usbdev->address == address) {
			bool in = media_callback_installed(usbdev->serial_usb);
			if (!in) {
				if (usbdev->status == in_mirror) {
					usb_device_reset(usbdev->serial_usb);
					return -1; //Android reset sometimes not work, force remove it;
				}
			} else {
				if (usbdev->status != in_mirror) {
					usbmuxd_log(LL_DEBUG, "Receive mirror start request, rescan device: %s", usbdev->serial_usb);
					send_state(usbdev, 0);
					usb_android_task_start(usbdev);
				} else {
					if (usbdev->first_send) {
						int64_t ts = os_gettime_ns();
						static int64_t last_ts = 0;

						if (ts - last_ts > 100000000) {
							last_ts = ts;
							uint8_t *fc = malloc(1);
							fc[0] = 2;
							if (usb_android_send(usbdev, fc, 1) < 0)
								free(fc);
						}
					}
				}
			}
			usbdev->alive = 1;
			found = 1;
			break;
		}
	}
	ENDFOREACH
	if (found)
		return 1;

	if (desc.bDeviceClass == 0x09)
		return -1;

	int res = -1;
	char serial[40];
	if ((res = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (uint8_t *)serial, 40)) <= 0) {
		usbmuxd_log(LL_WARNING, "Could not get the UDID for android device %d-%d: %s", bus, address, libusb_strerror(res));
		return -1;
	}

	struct libusb_config_descriptor *config;
	if ((res = libusb_get_active_config_descriptor(dev, &config)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not get configuration descriptor for device %d-%d: %s", bus, address, libusb_error_name(res));
		return -1;
	}

	struct usb_device *usbdev;
	usbdev = malloc(sizeof(struct usb_device));
	memset(usbdev, 0, sizeof(*usbdev));

	int interface_found = 0;
	for (int j = 0; j < config->bNumInterfaces; j++) {
		const struct libusb_interface *inter = &config->interface[j];
		if (inter == NULL) {
			continue;
		}

		for (j = 0; j < inter->num_altsetting; j++) {
			const struct libusb_interface_descriptor *interdesc = &inter->altsetting[j];
			if (interdesc->bNumEndpoints == 2 && interdesc->bInterfaceClass == 0xff && interdesc->bInterfaceSubClass == 0xff &&
			    (usbdev->ep_in <= 0 || usbdev->ep_out <= 0)) {
				for (int k = 0; k < (int)interdesc->bNumEndpoints; k++) {
					const struct libusb_endpoint_descriptor *epdesc = &interdesc->endpoint[k];
					if (epdesc->bmAttributes != 0x02) {
						break;
					}
					if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN) && usbdev->ep_in <= 0) {
						usbdev->ep_in = epdesc->bEndpointAddress;
					} else if ((!(epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)) && usbdev->ep_out <= 0) {
						usbdev->ep_out = epdesc->bEndpointAddress;
					} else {
						break;
					}
				}
				if (usbdev->ep_in && usbdev->ep_out) {
					usbdev->interface = interdesc->bInterfaceNumber;
					interface_found = 1;
				}
			}
		}
	}

	if (!interface_found) {
		usbmuxd_log(LL_WARNING, "Could not find a suitable USB interface for android device %d-%d", bus, address);
		libusb_free_config_descriptor(config);
		free(usbdev);
		return -1;
	}

	libusb_free_config_descriptor(config);

	if ((res = libusb_claim_interface(handle, usbdev->interface)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not claim interface %d for android device %d-%d: %s", usbdev->interface, bus, address, libusb_error_name(res));
		free(usbdev);
		return -1;
	}

	memcpy(usbdev->serial_usb, serial, 40);
	usbdev->serial[0] = 0;
	usbdev->bus = bus;
	usbdev->address = address;
	usbdev->devdesc = desc;
	usbdev->dev = handle;
	usbdev->alive = 1;
	usbdev->speed = 480000000;
	usbdev->wMaxPacketSize = libusb_get_max_packet_size(dev, usbdev->ep_out);
	if (usbdev->wMaxPacketSize <= 0) {
		usbmuxd_log(LL_ERROR, "Could not determine wMaxPacketSize for android device %d-%d, setting to 64", usbdev->bus, usbdev->address);
		usbdev->wMaxPacketSize = 64;
	} else {
		usbmuxd_log(LL_INFO, "Using wMaxPacketSize=%d for android device %d-%d", usbdev->wMaxPacketSize, usbdev->bus, usbdev->address);
	}

	collection_init(&usbdev->tx_xfers);
	collection_init(&usbdev->rx_xfers);
	collection_init(&usbdev->rx_xfers_for_mirror);

	usbdev->is_iOS = 0;
	collection_add(&device_list, usbdev);
	usbdev->status = not_in_mirror;

	return 1;
}

static int usb_device_add(libusb_device_handle *handle)
{
	int r = usb_android_device_add(handle);
	if (r != 0)
		return r;

	libusb_device *dev = libusb_get_device(handle);

	int j, res;
	// the following are non-blocking operations on the device list
	uint8_t bus = libusb_get_bus_number(dev);
	uint8_t address = libusb_get_device_address(dev);
	struct libusb_device_descriptor devdesc;
	struct libusb_transfer *transfer;
	int found = 0;
	FOREACH(struct usb_device * usbdev, &device_list)
	{
		if (usbdev->bus == bus && usbdev->address == address) {
			bool in = media_callback_installed(usbdev->serial_usb);
			if (in) { // request mirror, check mirror status whether need a mirror start
				if (usbdev->status != in_mirror) {
					usbmuxd_log(LL_DEBUG, "Receive mirror start request, rescan device: %s", usbdev->serial_usb);
					libusb_release_interface(usbdev->dev, usbdev->interface);
					return -1;
				}
			} else { // check mirror status, if in mirror, need stop mirror
				if (usbdev->status == in_mirror) {
					usb_device_reset(usbdev->serial_usb);
				}
			}
			usbdev->alive = 1;
			found = 1;
			break;
		}
	}
	ENDFOREACH
	if (found)
		return 0; //device already found

	if ((res = libusb_get_device_descriptor(dev, &devdesc)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not get device descriptor for device %d-%d: %s", bus, address, libusb_error_name(res));
		return -1;
	}

	usbmuxd_log(LL_INFO, "Found new device with v/p %04x:%04x at %d-%d", devdesc.idVendor, devdesc.idProduct, bus, address);

	int desired_config = devdesc.bNumConfigurations;
	int current_config = 0;
	if ((res = libusb_get_configuration(handle, &current_config)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not get configuration for device %d-%d: %s", bus, address, libusb_error_name(res));
		return -1;
	}

	char serial[40];
	if ((res = libusb_get_string_descriptor_ascii(handle, devdesc.iSerialNumber, (uint8_t *)serial, 40)) <= 0) {
		usbmuxd_log(LL_WARNING, "Could not get the UDID for device %d-%d: %s", bus, address, libusb_strerror(res));
		return -1;
	}

	bool mirror_request = media_callback_installed(serial);
	if (mirror_request && devdesc.bNumConfigurations != 5) {
		struct usb_device d;
		memset(&d, 0, sizeof(struct usb_device));
		memcpy(d.serial_usb, serial, strlen(serial));
		send_state(&d, 0);
		usb_win32_activate_quicktime(serial);
		return -2;
	}

	if (current_config != desired_config) {
#ifndef WIN32
		// Detaching the kernel driver is not a Win32-concept; this functionality is not implemented on Windows.
		struct libusb_config_descriptor *config;
		if ((res = libusb_get_active_config_descriptor(dev, &config)) != 0) {
			usbmuxd_log(LL_NOTICE, "Could not get old configuration descriptor for device %d-%d: %s", bus, address, libusb_error_name(res));
		} else {
			for (j = 0; j < config->bNumInterfaces; j++) {
				const struct libusb_interface_descriptor *intf = &config->interface[j].altsetting[0];
				if ((res = libusb_kernel_driver_active(handle, intf->bInterfaceNumber)) < 0) {
					usbmuxd_log(LL_NOTICE, "Could not check kernel ownership of interface %d for device %d-%d: %s", intf->bInterfaceNumber,
						    bus, address, libusb_error_name(res));
					continue;
				}
				if (res == 1) {
					usbmuxd_log(LL_INFO, "Detaching kernel driver for device %d-%d, interface %d", bus, address, intf->bInterfaceNumber);
					if ((res = libusb_detach_kernel_driver(handle, intf->bInterfaceNumber)) < 0) {
						usbmuxd_log(LL_WARNING, "Could not detach kernel driver, configuration change will probably fail! %s",
							    libusb_error_name(res));
						continue;
					}
				}
			}
			libusb_free_config_descriptor(config);
		}
#endif

		usbmuxd_log(LL_INFO, "Setting configuration for device %d-%d, from %d to %d", bus, address, current_config, desired_config);
#ifdef WIN32
		usb_win32_set_configuration(serial, (uint8_t)desired_config);

		// Because the change was done via libusb-win32, we need to refresh the device on libusb;
		// otherwise, it will not pick up the new configuration and endpoints.
		// For now, let the next loop do this for us.
		return -2;
#else
		if ((res = libusb_set_configuration(handle, desired_config)) != 0) {
			usbmuxd_log(LL_WARNING, "Could not set configuration %d for device %d-%d: %s", desired_config, bus, address, libusb_error_name(res));
			return -1;
		}
#endif
	}

	if (mirror_request)
		usb_win32_extra_cmd(serial);

	struct libusb_config_descriptor *config;
	if ((res = libusb_get_active_config_descriptor(dev, &config)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not get configuration descriptor for device %d-%d: %s", bus, address, libusb_error_name(res));
		return -1;
	}

	struct usb_device *usbdev;
	usbdev = malloc(sizeof(struct usb_device));
	memset(usbdev, 0, sizeof(*usbdev));

	int interface_found = 0, interface_found_fa = 0;
	for (j = 0; j < config->bNumInterfaces; j++) {
		const struct libusb_interface_descriptor *intf = &config->interface[j].altsetting[0];
		if (intf->bInterfaceClass == INTERFACE_CLASS && intf->bInterfaceSubClass == INTERFACE_SUBCLASS &&
		    intf->bInterfaceProtocol == INTERFACE_PROTOCOL) {
			if (intf->bNumEndpoints != 2) {
				usbmuxd_log(LL_WARNING, "Endpoint count mismatch for interface %d of device %d-%d", intf->bInterfaceNumber, bus, address);
				continue;
			}
			if ((intf->endpoint[0].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT &&
			    (intf->endpoint[1].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
				usbdev->interface = intf->bInterfaceNumber;
				usbdev->ep_out = intf->endpoint[0].bEndpointAddress;
				usbdev->ep_in = intf->endpoint[1].bEndpointAddress;
				interface_found = 1;
				usbmuxd_log(LL_INFO, "Found interface %d with endpoints %02x/%02x for device %d-%d", usbdev->interface, usbdev->ep_out,
					    usbdev->ep_in, bus, address);
			} else if ((intf->endpoint[1].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT &&
				   (intf->endpoint[0].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
				usbdev->interface = intf->bInterfaceNumber;
				usbdev->ep_out = intf->endpoint[1].bEndpointAddress;
				usbdev->ep_in = intf->endpoint[0].bEndpointAddress;
				interface_found = 1;
				usbmuxd_log(LL_INFO, "Found interface %d with swapped endpoints %02x/%02x for device %d-%d", usbdev->interface, usbdev->ep_out,
					    usbdev->ep_in, bus, address);
			} else {
				usbmuxd_log(LL_WARNING, "Endpoint type mismatch for interface %d of device %d-%d", intf->bInterfaceNumber, bus, address);
			}
		} else if (intf->bInterfaceClass == INTERFACE_CLASS && intf->bInterfaceSubClass == INTERFACE_QUICKTIMECLASS) {
			if (intf->bNumEndpoints != 2) {
				usbmuxd_log(LL_WARNING, "Endpoint count mismatch for interface %d of device %d-%d", intf->bInterfaceNumber, bus, address);
				continue;
			}
			if ((intf->endpoint[0].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT &&
			    (intf->endpoint[1].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
				usbdev->interface_fa = intf->bInterfaceNumber;
				usbdev->ep_out_fa = intf->endpoint[0].bEndpointAddress;
				usbdev->ep_in_fa = intf->endpoint[1].bEndpointAddress;
				interface_found_fa = 1;
			} else if ((intf->endpoint[1].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT &&
				   (intf->endpoint[0].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
				usbdev->interface_fa = intf->bInterfaceNumber;
				usbdev->ep_out_fa = intf->endpoint[1].bEndpointAddress;
				usbdev->ep_in_fa = intf->endpoint[0].bEndpointAddress;
				interface_found_fa = 1;
			} else {
				usbmuxd_log(LL_WARNING, "Endpoint type mismatch for interface %d of device %d-%d", intf->bInterfaceNumber, bus, address);
			}
		}
	}

	if (!interface_found || (mirror_request && !interface_found_fa)) {
		usbmuxd_log(LL_WARNING, "Could not find a suitable USB interface for device %d-%d", bus, address);
		libusb_free_config_descriptor(config);
		free(usbdev);
		return -1;
	}

	libusb_free_config_descriptor(config);

	if ((res = libusb_claim_interface(handle, usbdev->interface)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not claim interface %d for device %d-%d: %s", usbdev->interface, bus, address, libusb_error_name(res));
		free(usbdev);
		return -1;
	}

	if (mirror_request && (res = libusb_claim_interface(handle, usbdev->interface_fa)) != 0) {
		usbmuxd_log(LL_WARNING, "Could not claim interface_fa====>mirror %d for device %d-%d: %s", usbdev->interface_fa, bus, address,
			    libusb_error_name(res));
		free(usbdev);
		return -1;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		usbmuxd_log(LL_WARNING, "Failed to allocate transfer for device %d-%d: %s", bus, address, libusb_error_name(res));
		free(usbdev);
		return -1;
	}

	unsigned char *transfer_buffer = malloc(1024 + LIBUSB_CONTROL_SETUP_SIZE + 8);
	if (!transfer_buffer) {
		usbmuxd_log(LL_WARNING, "Failed to allocate transfer buffer for device %d-%d: %s", bus, address, libusb_error_name(res));
		free(usbdev);
		return -1;
	}
	memset(transfer_buffer, '\0', 1024 + LIBUSB_CONTROL_SETUP_SIZE + 8);

	memcpy(usbdev->serial_usb, serial, 40);
	usbdev->serial[0] = 0;
	usbdev->bus = bus;
	usbdev->address = address;
	usbdev->devdesc = devdesc;
	usbdev->speed = 480000000;
	usbdev->dev = handle;
	usbdev->alive = 1;
	usbdev->wMaxPacketSize = libusb_get_max_packet_size(dev, usbdev->ep_out);
	if (usbdev->wMaxPacketSize <= 0) {
		usbmuxd_log(LL_ERROR, "Could not determine wMaxPacketSize for device %d-%d, setting to 64", usbdev->bus, usbdev->address);
		usbdev->wMaxPacketSize = 64;
	} else {
		usbmuxd_log(LL_INFO, "Using wMaxPacketSize=%d for device %d-%d", usbdev->wMaxPacketSize, usbdev->bus, usbdev->address);
	}

	switch (libusb_get_device_speed(dev)) {
	case LIBUSB_SPEED_LOW:
		usbdev->speed = 1500000;
		break;
	case LIBUSB_SPEED_FULL:
		usbdev->speed = 12000000;
		break;
	case LIBUSB_SPEED_SUPER:
		usbdev->speed = 5000000000;
		break;
	case LIBUSB_SPEED_HIGH:
	case LIBUSB_SPEED_UNKNOWN:
	default:
		usbdev->speed = 480000000;
		break;
	}

	usbmuxd_log(LL_INFO, "USB Speed is %g MBit/s for device %d-%d", (double)(usbdev->speed / 1000000.0), usbdev->bus, usbdev->address);

	/**
	 * From libusb:
	 * 	Asking for the zero'th index is special - it returns a string
	 * 	descriptor that contains all the language IDs supported by the
	 * 	device.
	 **/
	libusb_fill_control_setup(transfer_buffer, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, LIBUSB_DT_STRING << 8, 0,
				  1024 + LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_transfer(transfer, handle, transfer_buffer, get_langid_callback, usbdev, 1000);

	if ((res = libusb_submit_transfer(transfer)) < 0) {
		usbmuxd_log(LL_ERROR, "Could not request transfer for device %d-%d: %s", usbdev->bus, usbdev->address, libusb_error_name(res));
		libusb_free_transfer(transfer);
		free(transfer_buffer);
		free(usbdev);
		return -1;
	}

	collection_init(&usbdev->tx_xfers);
	collection_init(&usbdev->rx_xfers);
	collection_init(&usbdev->rx_xfers_for_mirror);

	usbdev->is_iOS = 1;
	collection_add(&device_list, usbdev);

	if (mirror_request) {
		if (start_rx_loop_for_mirror(usbdev) < 0)
			usbmuxd_log(LL_WARNING, "Failed to start RX loop for mirror");
		else {
			usbmuxd_log(LL_DEBUG, "Success to start a new mirror task!!!");
			usbdev->status = in_mirror;
		}
	} else
		usbdev->status = not_in_mirror;

	return 0;
}

extern void lock_install();
extern void unlock_install();
int usb_discover(void)
{
	if (pthread_mutex_trylock(&devices_lock) != 0) {
		return 0;
	}

	// Mark all devices as dead, and do a mark-sweep like
	// collection of dead devices
	FOREACH(struct usb_device * usbdev, &device_list) { usbdev->alive = 0; }
	ENDFOREACH

	// Enumerate all USB devices and mark the ones we already know
	// about as live, again
	int valid_count = 0;
	FOREACH(libusb_device_handle * handle, &device_opened_handle_list)
	{
		if (usb_device_add(handle) >= 0)
			valid_count++;
	}
	ENDFOREACH

	FOREACH(libusb_device_handle * handle, &device_handle_list)
	{
		if (usb_device_add(handle) < 0) {
			libusb_close(handle);
			add_usb_device_change_event();
		} else {
			valid_count++;
			collection_add(&device_opened_handle_list, handle);
		}

		collection_remove(&device_handle_list, handle);
	}
	ENDFOREACH

	// Clean out any device we didn't mark back as live
	reap_dead_devices();

	get_tick_count(&next_dev_poll_time);
	next_dev_poll_time.tv_usec += DEVICE_POLL_TIME * 1000;
	next_dev_poll_time.tv_sec += next_dev_poll_time.tv_usec / 1000000;
	next_dev_poll_time.tv_usec = next_dev_poll_time.tv_usec % 1000000;

	pthread_mutex_unlock(&devices_lock);

	return valid_count;
}

pthread_t enum_th;
int enum_exit = 0;
extern int usb_find_devices(void);
static void *enum_proc(void *arg)
{
	(void *)arg;
	while (!enum_exit) {
		struct timeval now;
		struct timespec wait_time;
		pthread_mutex_lock(&wait_mutex);
		gettimeofday(&now, NULL);
		wait_time.tv_sec = now.tv_sec;
		wait_time.tv_nsec = now.tv_usec * 1000 + DEVICE_POLL_TIME * 1000000;
		pthread_cond_timedwait(&wait_cond, &wait_mutex, &wait_time);
		pthread_mutex_unlock(&wait_mutex);

		if (enum_exit)
			break;

		lock_install();
		if (!consume_usb_device_change_event()) {
			unlock_install();
			continue;
		}

		libusb_device **list;
		pthread_mutex_lock(&devices_lock);
		if (collection_count(&device_handle_list) == 0) {
			ssize_t devsCount = libusb_get_device_list(NULL, &list);
			for (ssize_t i = 0; i < devsCount; i++) {
				libusb_device_handle *opened_handle = NULL;
				FOREACH(libusb_device_handle * opened, &device_opened_handle_list)
				{
					libusb_device *device = libusb_get_device(opened);
					uint8_t bus = libusb_get_bus_number(device);
					uint8_t address = libusb_get_device_address(device);
					if (libusb_get_bus_number(list[i]) == bus && libusb_get_device_address(list[i]) == address) {
						opened_handle = opened;
						break;
					}
				}
				ENDFOREACH

				if (!opened_handle) {
					struct libusb_device_descriptor devdesc;
					if (libusb_get_device_descriptor(list[i], &devdesc) == LIBUSB_SUCCESS) {
						if (usb_is_aoa_device(devdesc.idVendor, devdesc.idProduct) ||
						    (devdesc.idVendor == VID_APPLE &&
						     (devdesc.idProduct == PID_APPLE_T2_COPROCESSOR ||
						      (devdesc.idProduct >= PID_RANGE_LOW && devdesc.idProduct <= PID_RANGE_MAX)))) {
							libusb_device_handle *handle = NULL;
							int res = libusb_open(list[i], &handle);
							if (res == LIBUSB_SUCCESS)
								collection_add(&device_handle_list, handle);
						}
					}
				}
			}
			libusb_free_device_list(list, 1);

			usb_find_devices();
		}

		pthread_mutex_unlock(&devices_lock);
		unlock_install();
	}

	return NULL;
}

void usb_enumerate_device()
{
	enum_exit = 0;
	pthread_create(&enum_th, NULL, enum_proc, NULL);
}

void usb_stop_enumerate()
{
	enum_exit = 1;
	pthread_mutex_lock(&wait_mutex);
	pthread_cond_signal(&wait_cond);
	pthread_mutex_unlock(&wait_mutex);
	pthread_join(enum_th, NULL);
}

const char *usb_get_serial(struct usb_device *dev)
{
	if (!dev->dev) {
		return NULL;
	}
	return dev->serial;
}

uint32_t usb_get_location(struct usb_device *dev)
{
	if (!dev->dev) {
		return 0;
	}
	return (dev->bus << 16) | dev->address;
}

uint16_t usb_get_pid(struct usb_device *dev)
{
	if (!dev->dev) {
		return 0;
	}
	return dev->devdesc.idProduct;
}

uint64_t usb_get_speed(struct usb_device *dev)
{
	if (!dev->dev) {
		return 0;
	}
	return dev->speed;
}

#ifndef WIN32
void usb_get_fds(struct fdlist *list)
{
	const struct libusb_pollfd **usbfds;
	const struct libusb_pollfd **p;
	usbfds = libusb_get_pollfds(NULL);
	if (!usbfds) {
		usbmuxd_log(LL_ERROR, "libusb_get_pollfds failed");
		return;
	}
	p = usbfds;
	while (*p) {
		fdlist_add(list, FD_USB, (*p)->fd, (*p)->events);
		p++;
	}
	free(usbfds);
}
#endif

void usb_autodiscover(int enable)
{
	usbmuxd_log(LL_DEBUG, "usb polling enable: %d", enable);
	device_polling = enable;
	device_hotplug = enable;
}

static int dev_poll_remain_ms(void)
{
	int msecs;
	struct timeval tv;
	if (!device_polling)
		return 100000; // devices will never be polled if this is > 0
	get_tick_count(&tv);
	msecs = (next_dev_poll_time.tv_sec - tv.tv_sec) * 1000;
	msecs += (next_dev_poll_time.tv_usec - tv.tv_usec) / 1000;
	if (msecs < 0)
		return 0;
	return msecs;
}

int usb_get_timeout(void)
{
	struct timeval tv;
	int msec;
	int res;
	int pollrem;
	pollrem = dev_poll_remain_ms();
	res = libusb_get_next_timeout(NULL, &tv);
	if (res == 0)
		return pollrem;
	if (res < 0) {
		usbmuxd_log(LL_ERROR, "libusb_get_next_timeout failed: %s", libusb_error_name(res));
		return pollrem;
	}
	msec = tv.tv_sec * 1000;
	msec += tv.tv_usec / 1000;
	if (msec > pollrem)
		return pollrem;
	return msec;
}

int usb_process(void)
{
	int res;
	struct timeval tv;
	tv.tv_sec = tv.tv_usec = 0;
	res = libusb_handle_events_timeout(NULL, &tv);
	if (res < 0) {
		usbmuxd_log(LL_ERROR, "libusb_handle_events_timeout failed: %s", libusb_error_name(res));
		return res;
	}

	// reap devices marked dead due to an RX error
	reap_dead_devices();

	if (dev_poll_remain_ms() <= 0) {
		res = usb_discover();
		if (res < 0) {
			usbmuxd_log(LL_ERROR, "usb_discover failed: %s", libusb_error_name(res));
			return res;
		}
	}
	return 0;
}

int usb_process_timeout(int msec)
{
	int res;
	struct timeval tleft, tcur, tfin;
	get_tick_count(&tcur);
	tfin.tv_sec = tcur.tv_sec + (msec / 1000);
	tfin.tv_usec = tcur.tv_usec + (msec % 1000) * 1000;
	tfin.tv_sec += tfin.tv_usec / 1000000;
	tfin.tv_usec %= 1000000;
	while ((tfin.tv_sec > tcur.tv_sec) || ((tfin.tv_sec == tcur.tv_sec) && (tfin.tv_usec > tcur.tv_usec))) {
		tleft.tv_sec = tfin.tv_sec - tcur.tv_sec;
		tleft.tv_usec = tfin.tv_usec - tcur.tv_usec;
		if (tleft.tv_usec < 0) {
			tleft.tv_usec += 1000000;
			tleft.tv_sec -= 1;
		}
		res = libusb_handle_events_timeout(NULL, &tleft);
		if (res < 0) {
			usbmuxd_log(LL_ERROR, "libusb_handle_events_timeout failed: %s", libusb_error_name(res));
			return res;
		}
		// reap devices marked dead due to an RX error
		reap_dead_devices();
		get_tick_count(&tcur);
	}
	return 0;
}

#ifdef HAVE_LIBUSB_HOTPLUG_API
static libusb_hotplug_callback_handle usb_hotplug_cb_handle;

static int LIBUSB_CALL usb_hotplug_cb(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
	if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
		if (device_hotplug) {
			usb_device_add(device);
		}
	} else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
		uint8_t bus = libusb_get_bus_number(device);
		uint8_t address = libusb_get_device_address(device);
		FOREACH(struct usb_device * usbdev, &device_list)
		{
			if (usbdev->bus == bus && usbdev->address == address) {
				usbdev->alive = 0;
				device_remove(usbdev);
				break;
			}
		}
		ENDFOREACH
	} else {
		usbmuxd_log(LL_ERROR, "Unhandled event %d", event);
	}
	return 0;
}
#endif

int usb_initialize(void)
{
	int res;
	const struct libusb_version *libusb_version_info = libusb_get_version();
	usbmuxd_log(LL_NOTICE, "Using libusb %d.%d.%d", libusb_version_info->major, libusb_version_info->minor, libusb_version_info->micro);

	devlist_failures = 0;
	device_polling = 1;
	res = libusb_init(NULL);

	if (res != 0) {
		usbmuxd_log(LL_FATAL, "libusb_init failed: %s", libusb_error_name(res));
		return -1;
	}

#if LIBUSB_API_VERSION >= 0x01000106
	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_NONE);
	/*libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL,
			  (log_level >= LL_DEBUG ? LIBUSB_LOG_LEVEL_DEBUG : (log_level >= LL_WARNING ? LIBUSB_LOG_LEVEL_WARNING : LIBUSB_LOG_LEVEL_NONE)));*/
#else
	libusb_set_debug(NULL, (log_level >= LL_DEBUG ? LIBUSB_LOG_LEVEL_DEBUG : (log_level >= LL_WARNING ? LIBUSB_LOG_LEVEL_WARNING : LIBUSB_LOG_LEVEL_NONE)));
#endif

#ifdef WIN32
	usb_win32_init();
#endif

	collection_init(&device_list);
	collection_init(&device_handle_list);
	collection_init(&device_opened_handle_list);

#ifdef HAVE_LIBUSB_HOTPLUG_API
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		usbmuxd_log(LL_INFO, "Registering for libusb hotplug events");
		res = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, LIBUSB_HOTPLUG_ENUMERATE,
						       VID_APPLE, LIBUSB_HOTPLUG_MATCH_ANY, 0, usb_hotplug_cb, NULL, &usb_hotplug_cb_handle);
		if (res == LIBUSB_SUCCESS) {
			device_polling = 0;
		} else {
			usbmuxd_log(LL_ERROR, "ERROR: Could not register for libusb hotplug events. %s", libusb_error_name(res));
		}
	} else {
		usbmuxd_log(LL_ERROR, "libusb does not support hotplug events");
	}
#endif
	if (device_polling) {
		res = usb_discover();
		if (res >= 0) {
		}
	} else {
		res = collection_count(&device_list);
	}
	return res;
}

void usb_shutdown(void)
{
	usbmuxd_log(LL_DEBUG, "usb_shutdown");

#ifdef HAVE_LIBUSB_HOTPLUG_API
	libusb_hotplug_deregister_callback(NULL, usb_hotplug_cb_handle);
#endif

	FOREACH(struct usb_device * usbdev, &device_list)
	{
		device_remove(usbdev);
		usb_disconnect(usbdev);
	}
	ENDFOREACH
	collection_free(&device_list);
	collection_free(&device_handle_list);
	collection_free(&device_opened_handle_list);
	libusb_exit(NULL);
}

void usb_camera_task_end(struct usb_device *dev)
{
	process_device_lost(dev->serial_usb);
}
