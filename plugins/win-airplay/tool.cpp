#include "tool.h"
#include <QFile>
#include <QStandardPaths>
#include "qrcode/QrToPng.h"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
QString streamUrlImage()
{
	QStringList ips;
	for (const QNetworkInterface &address : QNetworkInterface::allInterfaces()) {
		if (address.humanReadableName().contains("virtual", Qt::CaseInsensitive)
			|| address.humanReadableName().contains("vmware", Qt::CaseInsensitive))
			continue;

		if (address.type() != QNetworkInterface::Virtual
			&& address.type() != QNetworkInterface::Loopback
			&& address.type() != QNetworkInterface::Unknown) {
			QList<QNetworkAddressEntry> cc =
				address.addressEntries();
			for (auto iter = cc.begin(); iter != cc.end(); iter++)
			{
				QNetworkAddressEntry &entry = *iter;
				QHostAddress ad = entry.ip();
				if (ad.protocol() == QAbstractSocket::IPv4Protocol)
					ips.append(ad.toString());
			}
		}
	}
	QString str;
	for (int i=0; i<ips.size(); i++)
	{
		str.append(ips.at(i));
		if (i != ips.size() - 1)
			str.append("#");
	}

	if (str.isEmpty())
		return "";
	
	QString pngPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/xxq-stream-qrcode.png";
	auto exampleQrPng1 = QrToPng(pngPath.toStdString(), 200, 3, str.toStdString(), true, qrcodegen::QrCode::Ecc::MEDIUM);
	exampleQrPng1.writeToPNG();
	return pngPath;
}
