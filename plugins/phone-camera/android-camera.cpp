#include "android-camera.h"
#include <qdebug.h>
#include <qelapsedtimer.h>

extern QSet<QString> runningDevices;

AndroidCamera::AndroidCamera(QObject *parent) : QThread(parent) {}

AndroidCamera::~AndroidCamera()
{
	stopTask();
}

void AndroidCamera::setCurrentDevice(QString devicePath)
{
	if (devicePath == "disabled") {
		stopTask();
		m_devicePath.clear();
		return;
	}

	do {
		if (m_devicePath.isEmpty())
			break;

		if (devicePath == "auto")
			return;
		else {
			if (m_devicePath == "auto")
				break;
			else {
				if (m_devicePath == devicePath)
					return;
				else
					break;
			}
		}
	} while (1);

	m_devicePath = devicePath;
	stopTask();
}

void AndroidCamera::startTask(QString path)
{
	stopTask();

	if (m_devicePath != "auto" && !path.contains(serialNumber(m_devicePath)))
		return;

	m_connectedPath = path;
	m_running = true;
	start();
}

void AndroidCamera::stopTask()
{
	m_running = false;
	wait();
}

int AndroidCamera::setupDroid(libusb_device *usbDevice, libusb_device_handle *handle)
{
	auto device = &m_droid;
	memset(device, 0, sizeof(accessory_droid));

	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(usbDevice, &desc);
	if (r < 0) {
		qDebug("failed to get device descriptor\n");
		return r;
	}

	struct libusb_config_descriptor *config;
	r = libusb_get_config_descriptor(usbDevice, 0, &config);
	if (r < 0) {
		qDebug("failed t oget config descriptor\n");
		return r;
	}

	const struct libusb_interface *inter;
	const struct libusb_interface_descriptor *interdesc;
	const struct libusb_endpoint_descriptor *epdesc;
	int i, j, k;

	for (i = 0; i < (int)config->bNumInterfaces; i++) {
		qDebug("checking interface #%d\n", i);
		inter = &config->interface[i];
		if (inter == NULL) {
			qDebug("interface is null\n");
			continue;
		}
		qDebug("interface has %d alternate settings\n", inter->num_altsetting);
		for (j = 0; j < inter->num_altsetting; j++) {
			interdesc = &inter->altsetting[j];
			if (interdesc->bNumEndpoints == 2 && interdesc->bInterfaceClass == 0xff && interdesc->bInterfaceSubClass == 0xff &&
			    (device->inendp <= 0 || device->outendp <= 0)) {
				qDebug("interface %d is accessory candidate\n", i);
				for (k = 0; k < (int)interdesc->bNumEndpoints; k++) {
					epdesc = &interdesc->endpoint[k];
					if (epdesc->bmAttributes != 0x02) {
						break;
					}
					if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN) && device->inendp <= 0) {
						device->inendp = epdesc->bEndpointAddress;
						device->inpacketsize = epdesc->wMaxPacketSize;
						qDebug("using EP 0x%02x as bulk-in EP\n", (int)device->inendp);
					} else if ((!(epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)) && device->outendp <= 0) {
						device->outendp = epdesc->bEndpointAddress;
						device->outpacketsize = epdesc->wMaxPacketSize;
						qDebug("using EP 0x%02x as bulk-out EP\n", (int)device->outendp);
					} else {
						break;
					}
				}
				if (device->inendp && device->outendp) {
					device->bulkInterface = interdesc->bInterfaceNumber;
				}
			}
		}
	}

	if (!(device->inendp && device->outendp)) {
		qDebug("device has no accessory endpoints\n");
		return -2;
	}

	device->usbHandle = handle;
	r = libusb_claim_interface(device->usbHandle, device->bulkInterface);
	if (r < 0) {
		qDebug("failed to claim bulk interface\n");
		libusb_close(device->usbHandle);
		return r;
	}

	device->connected = true;
	device->c_vid = desc.idVendor;
	device->c_pid = desc.idProduct;

	return 0;
}

bool AndroidCamera::setupDevice(libusb_device *device, libusb_device_handle *handle)
{
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r < 0) {
		return false;
	}

	switch (desc.bDeviceClass) {
	case 0x09:
		return false;
	}

	r = setupDroid(device, handle);
	if (r < 0) {
		qDebug("failed to setup droid: %d\n", r);
		return false;
	}

	return true;
}

bool AndroidCamera::handleMediaData(circlebuf *buffer, uint8_t **cacheBuffer, size_t *cacheBufferSize)
{
	size_t headerSize = 4 + 8 + 1;

	if (buffer->size < headerSize)
		return false;

	size_t pktSize = 0;
	circlebuf_peek_front(buffer, &pktSize, 4);

	size_t totalSize = pktSize + headerSize;
	if (buffer->size < totalSize)
		return false;

	if (*cacheBufferSize < headerSize + pktSize) {
		*cacheBufferSize = headerSize + pktSize;
		*cacheBuffer = (uint8_t *)brealloc(*cacheBuffer, *cacheBufferSize);
	}

	circlebuf_pop_front(buffer, *cacheBuffer, headerSize);
	int type = (*cacheBuffer)[4];
	int64_t pts = 0;
	memcpy(&pts, (*cacheBuffer) + 5, 8);
	circlebuf_pop_front(buffer, *cacheBuffer, pktSize);

	if (type == 1) { //video
			 //bool is_pps = pts == ((int64_t)UINT64_C(0x8000000000000000));
		emit mediaData(*cacheBuffer, pktSize, pts, true);
	} else {
		/*struct av_packet_info pack_info = {0};
		pack_info.size = pktSize;
		pack_info.type = FFM_PACKET_AUDIO;
		pack_info.pts = pts * 1000000;
		ipc_client_write_2(m_client, &pack_info, sizeof(struct av_packet_info), m_cacheBuffer, pack_info.size, INFINITE);*/
	}
	return true;
}

void AndroidCamera::run()
{
	qDebug() << "android camera task start: " << m_connectedPath;

	runningDevices.insert(m_connectedPath);

	libusb_context *ctx = nullptr;
	libusb_device **devs = nullptr;
	int devsCount = 0;

	libusb_init(&ctx);
	libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_NONE);

	libusb_device_handle *handle = nullptr;
	libusb_device *device = nullptr;
	devsCount = libusb_get_device_list(ctx, &devs);
	for (int i = 0; i < devsCount; i++) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r < 0) {
			continue;
		}

		handle = nullptr;
		if (libusb_open(devs[i], &handle) == 0) {
			uint8_t buffer[256] = {0};
			if (libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, buffer, 255) >= 0) {
				if (!m_connectedPath.contains(QString((char *)buffer))) {
					libusb_close(handle);
					continue;
				} else {
					device = devs[i];
					break;
				}
			} else
				libusb_close(handle);
		}
	}

	if (handle && device) {
		if (setupDevice(device, handle)) {
			int r = -1;
			while (m_running) {
				int len = 0;
				unsigned char data = 1;
				r = libusb_bulk_transfer(m_droid.usbHandle, m_droid.outendp, &data, 1, &len, 100);

				if (r == LIBUSB_ERROR_TIMEOUT)
					continue;
				else
					break;
			}

			uint8_t *buffer = (uint8_t *)bmalloc(m_droid.inpacketsize);
			uint8_t *cacheBuffer = nullptr;
			size_t cacheBufferSize = 0;

			circlebuf mediaBuffer;
			circlebuf_init(&mediaBuffer);
			QElapsedTimer timer;
			timer.start();
			while (m_running) {
				if (timer.elapsed() > 100) {
					timer.start();
					int len = 0;
					uint8_t data[1] = {2};
					libusb_bulk_transfer(m_droid.usbHandle, m_droid.outendp, data, 1, &len, 10);
				}
				int len = 0;
				int r = libusb_bulk_transfer(m_droid.usbHandle, m_droid.inendp, buffer, m_droid.inpacketsize, &len, 200);
				if (r == 0) {
					circlebuf_push_back(&mediaBuffer, buffer, len);

					while (true) {
						bool b = handleMediaData(&mediaBuffer, &cacheBuffer, &cacheBufferSize);

						if (b)
							continue;
						else
							break;
					}
				} else if (r == LIBUSB_ERROR_TIMEOUT) {

					continue;
				} else
					break;
			}
			if (cacheBuffer)
				bfree(cacheBuffer);
			bfree(buffer);
			circlebuf_free(&mediaBuffer);
		}
	}

	if (handle)
		libusb_close(handle);

	if (devs) {
		libusb_free_device_list(devs, devsCount);
		devs = NULL;
	}

	if (ctx != NULL) {
		libusb_exit(ctx); //close the session
		ctx = NULL;
	}

	runningDevices.remove(m_connectedPath);
	m_connectedPath.clear();

	emit mediaFinish();

	qDebug() << "android camera end.";
}
