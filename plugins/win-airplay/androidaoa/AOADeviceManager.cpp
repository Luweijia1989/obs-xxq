#include "AOADeviceManager.h"
#include <QApplication>
#include <QDebug>
#include "../ipc.h"
#include "../common-define.h"


const char *vendor = "once2go";
const char *model = "HelperProjectionApp"; //根据这个model来打开手机的app
const char *description = u8"鱼耳直播助手";
const char *version = "1.0";
const char *uri = "https://www.douyu.com/client/mobile";
const char *serial = "0000000012345678";

AOADeviceManager::AOADeviceManager()
{
    m_driverHelper = new DriverHelper(this);
    connect(m_driverHelper, &DriverHelper::installProgress, this, &AOADeviceManager::installProgress);

    m_usbEventTimer = new QTimer(this);
    m_usbEventTimer->setSingleShot(false);
    connect(m_usbEventTimer, &QTimer::timeout, this, [=](){
        if (!m_ctx)
            return;

		struct timeval tv = { 0, 10000 };
        int r = libusb_handle_events_timeout(m_ctx, &tv);
        if (r) {
            if (r == LIBUSB_ERROR_INTERRUPTED) {
                // ignore
            } else {
                qDebug("libusb_handle_events_timeout: %d\n", r);
            }
        }
    });

    m_cacheBuffer = (uint8_t *)malloc(1024 * 1024);
    circlebuf_init(&m_mediaDataBuffer);
    ipc_client_create(&client);

    //h264.setFileName("E:\\android.h264");
    //h264.open(QFile::ReadWrite);
}

AOADeviceManager::~AOADeviceManager()
{
    disconnectDevice();

    circlebuf_free(&m_mediaDataBuffer);
    ipc_client_destroy(&client);
    free(m_cacheBuffer);
}

int AOADeviceManager::initUSB()
{
    int r;
    r = libusb_init(&m_ctx);
    if(r < 0) {
        qDebug() << "error init usb context";
        return r;
    }
    libusb_set_debug(m_ctx, 3);
    return 0;
}

void AOADeviceManager::clearUSB()
{
    if (m_devs) {
        libusb_free_device_list(m_devs, m_devs_count);
        m_devs = NULL;
    }

    if (m_ctx != NULL) {
        libusb_exit(m_ctx); //close the session
        m_ctx = NULL;
    }
}

int AOADeviceManager::setupDroid(libusb_device *usbDevice, accessory_droid *device) {

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
    int i,j,k;
	 
    for(i=0; i<(int)config->bNumInterfaces; i++) {
        qDebug("checking interface #%d\n", i);
        inter = &config->interface[i];
        if (inter == NULL) {
            qDebug("interface is null\n");
            continue;
        }
        qDebug("interface has %d alternate settings\n", inter->num_altsetting);
        for(j=0; j<inter->num_altsetting; j++) {
            interdesc = &inter->altsetting[j];
            if (interdesc->bNumEndpoints == 2
                    && interdesc->bInterfaceClass == 0xff
                    && interdesc->bInterfaceSubClass == 0xff &&
                    (device->inendp <= 0 || device->outendp <= 0)) {
                qDebug( "interface %d is accessory candidate\n", i);
                for(k=0; k < (int)interdesc->bNumEndpoints; k++) {
                    epdesc = &interdesc->endpoint[k];
                    if (epdesc->bmAttributes != 0x02) {
                        break;
                    }
                    if ((epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN) && device->inendp <= 0) {
                        device->inendp = epdesc->bEndpointAddress;
                        device->inpacketsize = epdesc->wMaxPacketSize;
                        qDebug( "using EP 0x%02x as bulk-in EP\n", (int)device->inendp);
                    } else if ((!(epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)) && device->outendp <= 0) {
                        device->outendp = epdesc->bEndpointAddress;
                        device->outpacketsize = epdesc->wMaxPacketSize;
                        qDebug( "using EP 0x%02x as bulk-out EP\n", (int)device->outendp);
                    } else {
                        break;
                    }
                }
                if (device->inendp && device->outendp) {
                    device->bulkInterface = interdesc->bInterfaceNumber;
                }
            } else if (interdesc->bInterfaceClass == 0x01
                       && interdesc->bInterfaceSubClass == 0x02
                       && interdesc->bNumEndpoints > 0
                       && device->audioendp <= 0) {

                qDebug( "interface %d is audio candidate\n", i);

                device->audioInterface = interdesc->bInterfaceNumber;
                device->audioAlternateSetting = interdesc->bAlternateSetting;

                for(k=0; k < (int)interdesc->bNumEndpoints; k++) {
                    epdesc = &interdesc->endpoint[k];
                    if (epdesc->bmAttributes != ((3 << 2) | (1 << 0))) {
                        qDebug("skipping non-iso ep\n");
                        break;
                    }
                    device->audioendp = epdesc->bEndpointAddress;
                    device->audiopacketsize = epdesc->wMaxPacketSize;
                    qDebug( "using EP 0x%02x as audio EP\n", (int)device->audioendp);
                    break;
                }
            }
        }
    }

    if (!(device->inendp && device->outendp)) {
        qDebug("device has no accessory endpoints\n");
        return -2;
    }

    r = libusb_open(usbDevice, &device->usbHandle);
    if(r < 0) {
        qDebug("failed to open usb handle\n");
        return r;
    }

    r = libusb_claim_interface(device->usbHandle, device->bulkInterface);
    if (r < 0) {
        qDebug("failed to claim bulk interface\n");
        libusb_close(device->usbHandle);
        return r;
    }

    if (device->audioendp) {
        r = libusb_claim_interface(device->usbHandle, device->audioInterface);
        if (r < 0) {
            qDebug("failed to claim audio interface\n");
            libusb_release_interface(device->usbHandle, device->bulkInterface);
            libusb_close(device->usbHandle);
            return r;
        }

        r = libusb_set_interface_alt_setting(device->usbHandle, device->audioInterface, device->audioAlternateSetting);
        if (r < 0) {
            qDebug("failed to set alternate setting\n");
            libusb_release_interface(device->usbHandle, device->bulkInterface);
            libusb_release_interface(device->usbHandle, device->audioInterface);
            libusb_close(device->usbHandle);
            return r;
        }

    }

    device->connected = true;
    device->c_vid = desc.idVendor;
    device->c_pid = desc.idProduct;

    return 0;
}

int AOADeviceManager::shutdownUSBDroid(accessory_droid *device) {

    if (device->audioendp)
        libusb_release_interface(device->usbHandle, device->audioInterface);

    if ((device->inendp && device->outendp))
        libusb_release_interface(device->usbHandle, device->bulkInterface);

    if(device->usbHandle != NULL)
        libusb_close(device->usbHandle);

    device->connected = false;
    device->c_vid = 0;
    device->c_pid = 0;

    return 0;
}

int AOADeviceManager::isDroidInAcc(libusb_device *dev) {
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
        qDebug("failed to get device descriptor\n");
        return 0;
    }

    if (desc.idVendor == VID_GOOGLE) {
        switch(desc.idProduct) {
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

void AOADeviceManager::switchDroidToAcc(libusb_device *dev, int force) {
    struct libusb_device_handle* handle;
    unsigned char ioBuffer[2];
    int r;
    int deviceProtocol;

    if(0 > libusb_open(dev, &handle)){
        qDebug("Failed to connect to device\n");
        return;
    }

    if(libusb_kernel_driver_active(handle, 0) > 0) {
        if(!force) {
            qDebug("kernel driver active, ignoring device");
            libusb_close(handle);
            return;
        }
        if(libusb_detach_kernel_driver(handle, 0)!=0) {
            qDebug("failed to detach kernel driver, ignoring device");
            libusb_close(handle);
            return;
        }
    }
    if(0> (r = libusb_control_transfer(handle,
                                       0xC0, //bmRequestType
                                       51, //Get Protocol
                                       0,
                                       0,
                                       ioBuffer,
                                       2,
                                       2000))) {
        qDebug("get protocol call failed\n");
        libusb_close(handle);
        return;
    }

    deviceProtocol = ioBuffer[1] << 8 | ioBuffer[0];
    if (deviceProtocol < AOA_PROTOCOL_MIN || deviceProtocol > AOA_PROTOCOL_MAX) {
        qDebug( "Unsupported AOA protocol %d\n", deviceProtocol);
        libusb_close(handle);
        return;
    }

    const char *setupStrings[6];
    setupStrings[0] = vendor;
    setupStrings[1] = model;
    setupStrings[2] = description;
    setupStrings[3] = version;
    setupStrings[4] = uri;
    setupStrings[5] = serial;

    int i;
    for(i=0;i<6;i++) {
        if(0 > (r = libusb_control_transfer(handle,
                                            0x40,
                                            52,
                                            0,
                                            (uint16_t)i,
                                            (unsigned char*)setupStrings[i],
                                            strlen(setupStrings[i]),2000))) {
            qDebug( "send string %d call failed\n", i);
            libusb_close(handle);
            return;
        }
    }

    if (deviceProtocol >= 2) {
        if(0 > (r = libusb_control_transfer(handle,
                                            0x40,
                                            58,
                                    #ifdef USE_AUDIO
                                            1, // 0=no audio, 1=2ch,16bit PCM, 44.1kHz
                                    #else
                                            0,
                                    #endif
                                            0,
                                            NULL,
                                            0,
                                            2000))) {
            qDebug( "set audio call failed\n");
            libusb_close(handle);
            return;
        }
    }

    if(0 > (r = libusb_control_transfer(handle,0x40,53,0,0,NULL,0,2000))) {
        qDebug( "start accessory call failed\n");
        libusb_close(handle);
        return;
    }

    libusb_close(handle);
}

static int initUsbXferThread(usbXferThread *t) {
    //	t->dead = 0;
    t->xfr = libusb_alloc_transfer(0);
    if (t->xfr == NULL) {
        return -1;
    }
    t->stop = 0;
    t->stopped = 0;
    t->usbActive = 0;
    t->tickle = 0;
    return 0;
}

static void destroyUsbXferThread(usbXferThread *t) {
    libusb_free_transfer(t->xfr);
}

void tickleUsbXferThread(usbXferThread *t) {
    t->mutex.lock();
    if(t->usbActive) {
        t->usbActive = 0;
        t->condition.wakeAll();
    }
    t->mutex.unlock();
}

static void WINAPI a2s_usbrx_cb(struct libusb_transfer *transfer) {
    usbXferThread *t = (usbXferThread*)transfer->user_data;
    tickleUsbXferThread(t);
}

void AOADeviceManager::checkAndSend()
{
	if (m_mediaDataBuffer.size > 0) {
		static bool send_h = false;
		if (!send_h) {
			struct av_packet_info pack_info = {0};
			pack_info.size = sizeof(struct media_info);
			pack_info.type = FFM_MEDIA_INFO;

			struct media_info info;
			info.pps_len = m_mediaDataBuffer.size;
			circlebuf_pop_front(&m_mediaDataBuffer, info.pps, info.pps_len);
			//memcpy(info.pps, buffer, len);

			ipc_client_write_2(client, &pack_info, sizeof(struct av_packet_info), &info, sizeof(struct media_info), INFINITE);
			qDebug() << "dddddddddddddd  " << pack_info.size;
			send_h = true;
		} else {
			struct av_packet_info pack_info = {0};
			pack_info.size = m_mediaDataBuffer.size;
			if (pack_info.size == 0)
				qDebug() << "CCCCCCCCCCCCCC  " << pack_info.size;
			else
				qDebug() << "FFFFFFFFFFFFFF  " << pack_info.size;
			pack_info.type = FFM_PACKET_VIDEO;
			pack_info.pts = 0;
			circlebuf_pop_front(&m_mediaDataBuffer, m_cacheBuffer, m_mediaDataBuffer.size);
			ipc_client_write_2(client, &pack_info, sizeof(struct av_packet_info), m_cacheBuffer, pack_info.size, INFINITE);
		}
	}
}

void *AOADeviceManager::a2s_usbRxThread( void *d ) {
    AOADeviceManager *device = (AOADeviceManager*)d;

    if (!device->buffer)
        device->buffer = (unsigned char *)malloc(device->m_droid.inpacketsize);

    int r;

    libusb_fill_bulk_transfer(device->m_usbRxThread.xfr, device->m_droid.usbHandle, device->m_droid.inendp,
                              device->buffer, device->m_droid.inpacketsize,
                              (libusb_transfer_cb_fn)&a2s_usbrx_cb, (void*)&device->m_usbRxThread, 0);

    while(!device->m_usbRxThread.stop && !device->m_usbDead) {
        device->m_usbRxThread.mutex.lock();
        device->m_usbRxThread.usbActive = 1;

        r = libusb_submit_transfer(device->m_usbRxThread.xfr);
        if (r < 0) {
            device->m_usbDead = 1;
            device->m_usbRxThread.usbActive = 0;
            device->m_usbRxThread.mutex.unlock();
            break;
        }

        device->m_usbRxThread.condition.wait(&device->m_usbRxThread.mutex);
        if (device->m_usbRxThread.usbActive) {
            qDebug("wait, unlock but usbActive!\n");
        }
        device->m_usbRxThread.mutex.unlock();

        if (device->m_usbRxThread.stop || device->m_usbDead)
            break;

        switch(device->m_usbRxThread.xfr->status) {
        case LIBUSB_TRANSFER_COMPLETED:
	    //device->h264.write((const char *)device->buffer, device->m_usbRxThread.xfr->actual_length);
	    if (device->m_usbRxThread.xfr->actual_length > 0) {
		circlebuf_push_back(&device->m_mediaDataBuffer, device->buffer, device->m_usbRxThread.xfr->actual_length);
		device->checkAndSend();
	    }

            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
        case LIBUSB_TRANSFER_ERROR:
            device->m_usbDead = 1;
            device->m_usbRxThread.stop = 1;
            break;
        default:
            break;
        }
    }

    device->m_usbRxThread.stopped = 1;

    QMetaObject::invokeMethod(device, "disconnectDevice", Qt::QueuedConnection);

    qDebug("a2s_usbRxThread finished\n");
    return NULL;
}

bool AOADeviceManager::isAndroidDevice(uint16_t vid, uint16_t pid)
{
    Q_UNUSED(pid)
    return vid == 0x2717 || vid == 0x18D1;
}


int AOADeviceManager::startUSBPipe() {
    if(initUsbXferThread(&m_usbRxThread) < 0) {
        qDebug("failed to allocate usb rx transfer\n");
        return -1;
    }

    m_usbRxThread.thread = std::thread(AOADeviceManager::a2s_usbRxThread, this);
    m_usbRxThread.thread.detach();

    m_usbEventTimer->start(1);
    return 0;
}

void AOADeviceManager::stopUSBPipe() {
	m_usbEventTimer->stop();

	if (m_usbRxThread.stopped != 1) {
		m_usbRxThread.stop = 1;
		if(m_usbRxThread.usbActive) {
			libusb_cancel_transfer(m_usbRxThread.xfr);
		}
		qDebug("usb rx thread tickle\n");
		tickleUsbXferThread(&m_usbRxThread);
	}
    qDebug("waiting for usb rx thread...\n");
    if (m_usbRxThread.thread.joinable())
        m_usbRxThread.thread.join();

    qDebug("usb rx thread destroy!\n");
    destroyUsbXferThread(&m_usbRxThread);

    qDebug("threads stopped\n");

   // h264.close();
}



int AOADeviceManager::connectDevice(libusb_device *device)
{
    struct libusb_device_descriptor desc;
    int r = libusb_get_device_descriptor(device, &desc);
    if (r < 0) {
        return -1;
    }

    qDebug() << "usb device vid: " << QString::number(desc.idVendor, 16);
    switch(desc.bDeviceClass) {
    case 0x09:
        return -1;
    }

    r = setupDroid(device, &m_droid);
    if (r < 0) {
        qDebug("failed to setup droid: %d\n", r);
        return -3;
    }

    r = startUSBPipe();
    if (r < 0) {
        qDebug("failed to start pipe: %d", r);
        disconnectDevice();
        return -5;
    }

    qDebug("new Android connected");
    return 0;
}

void AOADeviceManager::disconnectDevice()
{
    stopUSBPipe();
    shutdownUSBDroid(&m_droid);
    m_usbDead = 0;
    clearUSB();

    qDebug("Android disconnected");
}

bool AOADeviceManager::enumDeviceAndCheck()
{
    bool ret = false;
    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) < 0)
        return ret;

    libusb_device **devs = nullptr;
    int count = libusb_get_device_list(ctx, &devs);
    for (int i=0; i<count; i++) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) {
            continue;
        }

        bool isAndroid = isAndroidDevice(desc.idVendor, desc.idProduct);
        if (isAndroid) {
            ret = true;
            break;
        }
    }

    libusb_free_device_list(devs, count);
    libusb_exit(ctx);

    return ret;
}

void AOADeviceManager::checkAndInstallDriver()
{
    libusb_context *ctx = nullptr;
    libusb_device **devs = nullptr;
    int count = 0;

Retry:
    if (devs)
        libusb_free_device_list(devs, count);
    if (ctx)
        libusb_exit(ctx);

    if (libusb_init(&ctx) < 0)
        return;

    count = libusb_get_device_list(ctx, &devs);
    for (int i=0; i<count; i++) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) {
            continue;
        }

        if (isAndroidDevice(desc.idVendor, desc.idProduct)) {
            if (m_driverHelper->checkInstall(desc.idVendor, desc.idProduct)) {
                m_waitMutex.lock();
                m_waitCondition.wait(&m_waitMutex, 500);
                m_waitMutex.unlock();
                goto Retry;
            }
            if (isDroidInAcc(devs[i])) {
                break;
            } else {
                switchDroidToAcc(devs[i], 1);
                m_waitMutex.lock();
                m_waitCondition.wait(&m_waitMutex, 500);
                m_waitMutex.unlock();
                goto Retry;
            }
        }
    }

    libusb_free_device_list(devs, count);
    libusb_exit(ctx);
}

bool AOADeviceManager::startTask()
{
    clearUSB();

    bool ret = false;
    if (initUSB() < 0)
        return ret;

    m_devs_count = libusb_get_device_list(m_ctx, &m_devs);
    for (int i=0; i<m_devs_count; i++) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(m_devs[i], &desc);
        if (r < 0) {
            continue;
        }

        if (isDroidInAcc(m_devs[i])) {
            ret = connectDevice(m_devs[i]) == 0;
            break;
        }
    }

    return ret;
}

void AOADeviceManager::updateUsbInventory()
{
    inUpdate = true;
    bool exist = enumDeviceAndCheck();
    if (exist && !m_droid.connected) {
        checkAndInstallDriver();
        startTask();
    }
    inUpdate = false;
}
