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
	if (m_devicePath == devicePath)
		return;

	m_devicePath = devicePath;
	stopTask();
}

void AndroidCamera::startTask(QString path)
{
	stopTask();

	if (!path.contains(serialNumber(m_devicePath)))
		return;

	m_running = true;
	start();
}

void AndroidCamera::stopTask()
{
	m_running = false;
	wait();

	if (m_cacheBuffer) {
		free(m_cacheBuffer);
		m_cacheBuffer = nullptr;
	}
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

bool AndroidCamera::handleMediaData(circlebuf *buffer)
{
	size_t headerSize = 4 + 8 + 1;

	if (buffer->size < headerSize)
		return false;

	size_t pktSize = 0;
	circlebuf_peek_front(buffer, &pktSize, 4);

	size_t totalSize = pktSize + headerSize;
	if (buffer->size < totalSize)
		return false;

	if (!m_cacheBuffer) {
		m_cacheBufferSize = headerSize + pktSize;
		m_cacheBuffer = (uint8_t *)malloc(m_cacheBufferSize);
	} else if (m_cacheBufferSize < headerSize + pktSize) {
		m_cacheBufferSize = headerSize + pktSize;
		m_cacheBuffer = (uint8_t *)realloc(m_cacheBuffer, m_cacheBufferSize);
	}

	circlebuf_pop_front(buffer, m_cacheBuffer, headerSize);
	int type = m_cacheBuffer[4];
	int64_t pts = 0;
	memcpy(&pts, m_cacheBuffer + 5, 8);
	circlebuf_pop_front(buffer, m_cacheBuffer, pktSize);

	if (type == 1) { //video
		bool is_pps = pts == ((int64_t)UINT64_C(0x8000000000000000));
		if (is_pps) {
			emit mediaData(m_cacheBuffer, pktSize, pts, true);
			/*struct av_packet_info pack_info = {0};
			pack_info.size = sizeof(struct media_video_info);
			pack_info.type = FFM_MEDIA_VIDEO_INFO;
			pack_info.pts = 0;

			struct media_video_info info;
			info.video_extra_len = pktSize;
			memcpy(info.video_extra, m_cacheBuffer, pktSize);
			ipc_client_write_2(m_client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_video_info), INFINITE);

			struct media_audio_info audio_info;
			audio_info.format = AUDIO_FORMAT_16BIT;
			audio_info.samples_per_sec = 48000;
			audio_info.speakers = SPEAKERS_STEREO;

			struct av_packet_info audio_pack_info = {0};
			audio_pack_info.size = sizeof(struct media_audio_info);
			audio_pack_info.type = FFM_MEDIA_AUDIO_INFO;
			ipc_client_write_2(m_client, &audio_pack_info, sizeof(struct av_packet_info), &audio_info, sizeof(struct media_audio_info), INFINITE);*/
		} else {
			emit mediaData(m_cacheBuffer, pktSize, pts, true);
			/*struct av_packet_info pack_info = {0};
			pack_info.size = pktSize;
			pack_info.type = FFM_PACKET_VIDEO;
			pack_info.pts = pts * 1000000;
			ipc_client_write_2(m_client, &pack_info, sizeof(struct av_packet_info), m_cacheBuffer, pack_info.size, INFINITE);*/
		}
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
	qDebug() << "android camera task start: " << m_devicePath;

	runningDevices.insert(m_devicePath);

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
				if (!m_devicePath.contains(QString((char *)buffer))) {
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

			uint8_t *buffer = (uint8_t *)malloc(m_droid.inpacketsize);

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
						bool b = handleMediaData(&mediaBuffer);

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

	runningDevices.remove(m_devicePath);

	emit mediaFinish();

	qDebug() << "android camera end.";
}
