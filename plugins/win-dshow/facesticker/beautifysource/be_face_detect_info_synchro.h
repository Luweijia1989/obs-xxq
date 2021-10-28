#ifndef BE_FACE_DETECT_INFO_SYNCHRO_H
#define BE_FACE_DETECT_INFO_SYNCHRO_H

#include <QObject>
#include <string>

#include "bef_effect_ai_face_detect.h"

class BEFaceDetectInfoSynchro :public QObject {
    Q_OBJECT
    Q_PROPERTY(QString faceYaw READ  faceYaw WRITE setFaceYaw NOTIFY faceYawChanged)
    Q_PROPERTY(QString faceRoll READ faceRoll WRITE setFaceRoll NOTIFY faceRollChanged)
    Q_PROPERTY(QString facePitch READ facePitch WRITE setFacePitch NOTIFY facePitchChanged)

    Q_PROPERTY(bool faceEyeBlinkStatus READ faceEyeBlinkStatus WRITE setFaceEyeBlinkStatus NOTIFY faceEyeBlinkStatusChanged)

    Q_PROPERTY(bool facePitchHeadStatus READ facePitchHeadStatus WRITE setFacePitchHeadStatus NOTIFY facePitchHeadStatusChanged)
    Q_PROPERTY(bool faceOpenMouthStatus READ faceOpenMouthStatus WRITE setFaceOpenMouthStatus NOTIFY faceOpenMouthStatusChanged)
    Q_PROPERTY(bool facePoutMouthStatus READ facePoutMouthStatus WRITE setFacePoutMouthStatus NOTIFY facePoutMouthStatusChanged)
    Q_PROPERTY(bool faceJumpBrowStatus READ faceJumpBrowStatus WRITE setFaceJumpBrowStatus NOTIFY faceJumpBrowStatusChanged)
    Q_PROPERTY(bool faceYawHeadStatus READ faceYawHeadStatus WRITE setFaceYawHeadStatus NOTIFY faceYawHeadStatusChanged)

public:
    void clearDetectCount();
signals:
    void detectCountChanged(QString val); 
    void faceCountChanged(QString val);
    void faceYawChanged(QString val); 
    void faceRollChanged(QString val); 
    void facePitchChanged(QString val); 

    void faceEyeBlinkStatusChanged(bool val);
    void facePitchHeadStatusChanged(bool val);
    void faceOpenMouthStatusChanged(bool val);
    void facePoutMouthStatusChanged(bool val);
    void faceJumpBrowStatusChanged(bool val);
    void faceYawHeadStatusChanged(bool val);

    void faceVerify(int status, QString imgFile);
    void faceVerifyResult(QString score, bool sameFace);

public:
    void syncFaceDetectInfo(bef_ai_face_info *faceInfo);
    void syncFaceVerify(int status, QString imgFile);
    void syncFaceVerifyResult(qreal score);

public:
    QString faceYaw() { return m_yawAngle; }
    QString faceRoll() { return m_rollAngle; }
    QString facePitch() { return m_pitchAngle; }

    void setFaceYaw(QString val) { m_yawAngle = val; }
    void setFaceRoll(QString val) { m_rollAngle = val; }
    void setFacePitch(QString val) { m_pitchAngle = val; }

    bool faceEyeBlinkStatus() { return m_eyeBlinkStatus; }
    bool facePitchHeadStatus() { return m_pitchHeadStatus; }
    bool faceOpenMouthStatus() { return m_openMouthStatus; }
    bool facePoutMouthStatus() { return m_poutMouthStatus; }
    bool faceJumpBrowStatus() { return m_jumpBrowStatus; }
    bool faceYawHeadStatus() { return m_yawHeadStatus; }

    void setFaceEyeBlinkStatus(bool val) { m_eyeBlinkStatus = val; }
    void setFacePitchHeadStatus(bool val) { m_pitchHeadStatus = val; }
    void setFaceOpenMouthStatus(bool val) { m_openMouthStatus = val; }
    void setFacePoutMouthStatus(bool val) { m_poutMouthStatus = val; }
    void setFaceJumpBrowStatus(bool val) { m_jumpBrowStatus = val; }
    void setFaceYawHeadStatus(bool val) { m_yawHeadStatus = val; }
    
private:
    QString m_yawAngle = "";
    QString m_rollAngle = "";
    QString m_pitchAngle = "";

    int m_detectCount = 0;
    int m_lastDetectCount = 0;

    bool m_eyeBlinkStatus = false;
    bool m_pitchHeadStatus = false;
    bool m_openMouthStatus = false;
    bool m_poutMouthStatus = false;
    bool m_jumpBrowStatus = false;
    bool m_yawHeadStatus = false;
};


#endif //BE_FACE_DETECT_INFO_SYNCHRO_H
