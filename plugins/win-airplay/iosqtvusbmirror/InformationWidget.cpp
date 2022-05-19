#include "InformationWidget.h"
#include <QProgressBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QApplication>

InformationWidget::InformationWidget(QWidget *parent) : QWidget(parent)
{
	setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint |
		       Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
	setFixedWidth(280);
	QVBoxLayout *ly = new QVBoxLayout(this);
	ly->setMargin(0);
	ly->setSpacing(0);

	m_tipLabel = new QLabel(u8"正在安装驱动");
	ly->addWidget(m_tipLabel);

	m_progressBar = new QProgressBar;
	m_progressBar->setOrientation(Qt::Horizontal);
	m_progressBar->setRange(0, 100);
	ly->addWidget(m_progressBar);

	setVisible(false);

	m_tipTimer = new QTimer(this);
	m_tipTimer->setSingleShot(true);

	QTimer *t = new QTimer(this);
	connect(t, &QTimer::timeout, this, [=]() {
	});
	t->start(100);
}

void InformationWidget::onInstallStatus(int value)
{
	bool show = true;
	if (m_progressBar) {
		int v = 0;
		switch (value) {
		case 0: //开始安装驱动
			m_tipLabel->setText(u8"正在安装驱动");
			v = 0;
			break;
		case 1: //安装一阶段
			v = 40;
			break;
		case 2: //安装二阶段，释放驱动
			v = 75;
			break;
		case 3: //安装三阶段，成功
			v = 100;
			show = false;
			break;
		}

		m_progressBar->setValue(v);
		setVisible(show);
	}
}

void InformationWidget::onInstallError(QString msg)
{
	m_tipLabel->setText(QString(u8"正在安装驱动(%1)").arg(msg));
}

void InformationWidget::onInfoPrompt(const QString &msg)
{
	if (m_tipTimer->isActive())
		return;

	m_tipTimer->start(5000);
	QMessageBox dlg(QMessageBox::Information, u8"提示", msg);
	dlg.setWindowFlags(dlg.windowFlags() | Qt::WindowStaysOnTopHint);
	dlg.exec();
}

void InformationWidget::onDeviceLost()
{
	setVisible(false);
}
