#ifndef BE_HAND_DETECT_INFO_SYNCHRO_H
#define BE_HAND_DETECT_INFO_SYNCHRO_H

#include <QObject>
#include <QString>
#include <string>

#include "bef_effect_ai_hand.h"

class BEHandDetectInfoSynchro : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString handGesture READ handGesture WRITE setHandGesture NOTIFY handGestureChanged)
       
public:
    BEHandDetectInfoSynchro();
    void syncHandDetectInfo(bef_ai_hand_info * handInfo);
signals:
    void handGestureChanged(QString val);
public:
    QString handGesture() { return m_handGesture; }
    void setHandGesture(QString handGes) { m_handGesture = handGes; }
private:
    void registerHandGestureType();

private:
    QString m_handGesture;
    std::map<unsigned int, QString> m_handGestureMap;
};

#endif//BE_HAND_DETECT_INFO_SYNCHRO_H
