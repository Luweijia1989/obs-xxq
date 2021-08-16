#ifndef INFORMATIONWIDGET_H
#define INFORMATIONWIDGET_H

#include <QWidget>

class QProgressBar;
class InformationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InformationWidget(QWidget *parent = nullptr);

signals:

public slots:
    void onInstallStatus(int step, int value);

private:
    QProgressBar *m_progressBar = nullptr;

};

#endif // INFORMATIONWIDGET_H
