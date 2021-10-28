#ifndef BE_FACE_ATTR_INFO_SYNCHRO_H
#define BE_FACE_ATTR_INFO_SYNCHRO_H

#include <QObject>
#include <QString>
#include <string>

#include "bef_effect_ai_face_attribute.h"
class BEFaceAttrInfoSynchro : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString faceAttrExpression READ faceAttrExpression WRITE setFaceAttrExpression NOTIFY faceAttrExpressionChanged)

public:
    BEFaceAttrInfoSynchro();
    void syncFaceAttrInfo(bef_ai_face_attribute_info * faceAttrInfo);
signals:
    void faceAttrExpressionChanged(QString val);
    void faceAttrOtherInfoChanged(bool bEmpty, int age, int gender, int beauty, int happiness, bool bConfuse);
public:
    QString faceAttrExpression() { return m_attrExpression; }
    void setFaceAttrExpression(QString exp) { m_attrExpression = exp; }
private:
    void registerExpType();

private:
    QString m_attrExpression;
    std::map<bef_ai_face_attribute_expression_type, QString> m_expStrMap;
};

#endif//BE_FACE_ATTR_INFO_SYNCHRO_H
