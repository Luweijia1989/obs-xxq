#ifndef DRIVERHELPER_H
#define DRIVERHELPER_H

#include <QObject>
#include <libwdi.h>

struct libusb_context;
struct libusb_device;

class DriverHelper : public QObject
{
    Q_OBJECT
public:
    explicit DriverHelper(QObject *parent = nullptr);
    bool checkInstall(int vid, int pid, QString targetDevicePath);

private:
    void install(wdi_device_info *dev);

signals:
    void installProgress(int value);
    void installError(QString msg);

private:
    QString temp_dir;
    VS_FIXEDFILEINFO file_info;
    UINT64 target_driver_version = 0;
    wdi_options_prepare_driver pd_options = {0};
    wdi_options_install_driver id_options = {0};

    bool inInstall = false;
};

#endif // DRIVERHELPER_H
