#ifndef INFORMATIONWIDGET_H
#define INFORMATIONWIDGET_H

#include <QWidget>

class QProgressBar;
class QTimer;
class InformationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InformationWidget(QWidget *parent = nullptr);

signals:

public slots:
    void onInstallStatus(int step, int value);
    void onInfoPrompt(const QString &msg);

private:
    QProgressBar *m_progressBar = nullptr;
    QTimer *m_showTimer;
};

#endif // INFORMATIONWIDGET_H
