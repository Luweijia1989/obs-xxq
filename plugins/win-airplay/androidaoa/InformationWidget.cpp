#include "InformationWidget.h"
#include <QProgressBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>

InformationWidget::InformationWidget(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint |
                                     Qt::WindowTitleHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setFixedWidth(280);
    QVBoxLayout *ly = new QVBoxLayout(this);
    ly->setMargin(0);
    ly->setSpacing(0);

    QLabel *lb = new QLabel(u8"正在安装驱动");
    ly->addWidget(lb);

    m_progressBar = new QProgressBar;
    m_progressBar->setOrientation(Qt::Horizontal);
    m_progressBar->setRange(0, 100);
    ly->addWidget(m_progressBar);

    setVisible(false);

    m_showTimer = new QTimer(this);
    m_showTimer->setSingleShot(true);
    m_showTimer->setInterval(3000);
    connect(m_showTimer, &QTimer::timeout, this, [=](){
	setVisible(true);
    });
}

void InformationWidget::onInstallStatus(int step, int value)
{
    bool delayShow = false;
    if (value == -1) {//失败
        setVisible(false);
    } else {
        if (m_progressBar)
        {
            int v = 0;
            switch (value) {
            case 0: //开始安装驱动
                v = step == 0 ? 0 : 50;
                break;
            case 1: //安装一阶段
                v = step == 0 ? 10 : 60;
                break;
            case 2: //安装二阶段，释放驱动
                v = step == 0 ? 25 : 75;
                break;
            case 3: //安装三阶段，成功
		if (step == 0)
		    delayShow = true;
                v = step == 0 ? 50 : 100;
                break;
            }

            m_progressBar->setValue(v);
	    if (delayShow)
		m_showTimer->start();
	    else {
                setVisible(m_progressBar->value() != 100);
		if (m_progressBar->value() == 100)
			m_showTimer->stop();
	    }
        }
    }
}
