#ifndef DRIVERHELPER_H
#define DRIVERHELPER_H

#include <QObject>
#include <libwdi.h>

struct libusb_context;
struct libusb_device;

class DriverHelper : public QObject {
	Q_OBJECT
public:
	explicit DriverHelper(QObject *parent = nullptr);
	Q_INVOKABLE void checkInstall(bool isAppleDevice, int vid, int pid, QString targetDevicePath);

private:
	bool install(wdi_device_info *dev);

signals:
	void installProgress(int step);
	void installError(QString msg);
	void changeTip(QString tip);
	void complete(int status);

private:
	QString temp_dir;
	VS_FIXEDFILEINFO file_info;
	UINT64 target_driver_version = 0;
	wdi_options_prepare_driver pd_options = {0};
	wdi_options_install_driver id_options = {0};
};

#endif // DRIVERHELPER_H
