#include "tool.h"
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include "qrcode/QrToPng.h"

#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
QStringList streamUrlImages()
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
	QStringList ret;

	auto time = QDateTime::currentMSecsSinceEpoch();
	for (int i=0; i<ips.size(); i++)
	{
		QString pngPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/xxq-stream-qrcode.png";
		auto url = QString("rtmp://%1/xxq-live/%2").arg(ips[i]).arg(time);
		auto exampleQrPng1 = QrToPng(pngPath.toStdString(), 600, 3, url.toStdString(), true, qrcodegen::QrCode::Ecc::MEDIUM);
		exampleQrPng1.writeToPNG();

		QImage image(600, 800, QImage::Format_RGBA8888);
		image.fill(Qt::transparent);
		QPainter p(&image);
		p.drawImage(QRect(0, 0, 600, 600), QImage(pngPath));
		QFont f;
		f.setPixelSize(32);
		QPen pen;
		pen.setColor(Qt::white);
		p.setFont(f);
		p.setPen(pen);
		p.drawText(QRect(0, 600, 600, 200), Qt::AlignCenter, u8"打开鱼耳主播助手APP，扫一扫完成投屏");
		QString savePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QString("/xxq-stream-url%1.png").arg(i);
		image.save(savePath);
		ret.append(savePath);
	}

	return ret;
}
