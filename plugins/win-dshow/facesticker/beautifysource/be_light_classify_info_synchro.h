#ifndef BE_LIGHT_CLASSIFY_INFO_SYNCHRO_H
#define BE_LIGHT_CLASSIFY_INFO_SYNCHRO_H

#include <QObject>
#include <QString>
#include <string>

//#include "bef_effect_ai_light_classify.h"
#include "bef_effect_ai_lightcls.h"

typedef enum {
    BEF_LIGHT_TYPE_INDOOR_YELLOW = 0,
    BEF_LIGHT_TYPE_INDOOR_WHITE,
    BEF_LIGHT_TYPE_INDOOR_WEAK,
    BEF_LIGHT_TYPE_SUNNY,
    BEF_LIGHT_TYPE_CLOUDY,
    BEF_LIGHT_TYPE_NIGHT,
    BEF_LIGHT_TYPE_BLACKLIGHT,
}bef_light_classify_type;

class BELightClassifyInfoSynchro : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lightClassifyType READ lightClassifyType WRITE setLightClassifyType NOTIFY lightClassifyTypeChanged)
    Q_PROPERTY(QString lightClassifyProb READ lightClassifyProb WRITE setLightClassifyProb NOTIFY lightClassifyProbChanged)

public:
    BELightClassifyInfoSynchro();
    void syncLightClassifyInfo(bef_ai_light_cls_result* lightClsInfo);

signals:
    void lightClassifyTypeChanged(QString val);
    void lightClassifyProbChanged(QString val);

public:
    QString lightClassifyType() { return m_lightClsType; }
    QString lightClassifyProb() { return m_lightClsProb; }
    void setLightClassifyType(QString val) { m_lightClsType = val; }
    void setLightClassifyProb(QString val) { m_lightClsProb = val; }
private:
    void registerLightClsType();

private:
    QString m_lightClsType;
    QString m_lightClsProb;
    std::map<bef_light_classify_type, QString> m_lightClsTypeMap;
};

#endif//BE_LIGHT_CLASSIFY_INFO_SYNCHRO_H
