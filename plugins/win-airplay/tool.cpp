#include "tool.h"
#include <QFile>
#include <QStandardPaths>
#include "qrcode/QrToPng.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
QString streamUrlImage()
{
	QJsonArray arr;
	for (const QNetworkInterface &address : QNetworkInterface::allInterfaces()) {
		if (address.humanReadableName().contains("virtual", Qt::CaseInsensitive)
			|| address.humanReadableName().contains("vmware", Qt::CaseInsensitive))
			continue;

		if (address.type() != QNetworkInterface::Virtual
			&& address.type() != QNetworkInterface::Loopback
			&& address.type() != QNetworkInterface::Unknown) {
			QList<QNetworkAddressEntry> cc =
				address.addressEntries();
			for (auto iter = cc.begin(); iter != cc.end(); iter++) {
				QNetworkAddressEntry &entry = *iter;
				QHostAddress ad = entry.ip();
				if (ad.protocol() == QAbstractSocket::IPv4Protocol) {
					auto ipstr = ad.toString();
					if(ipstr.startsWith("192") || ipstr.startsWith("172") || ipstr.startsWith("10"))
						arr.append(ad.toString());
				}
			}
		}
	}

	QJsonDocument jd(arr);
	QString s = jd.toJson(QJsonDocument::Compact);
	
	QString pngPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/xxq-stream-qrcode.png";
	auto exampleQrPng1 = QrToPng(pngPath.toStdString(), 200, 3, s.toStdString(), true, qrcodegen::QrCode::Ecc::MEDIUM);
	exampleQrPng1.writeToPNG();
	return pngPath;
}
