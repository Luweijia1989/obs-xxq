#include "be_face_detect_info_synchro.h"


static const float SAME_FACE_SCORE = 67.6;

void BEFaceDetectInfoSynchro::syncFaceDetectInfo(bef_ai_face_info *faceInfo) {
    
    int count = 0;
    if(faceInfo) {
        count = faceInfo->face_count;
    }

    if(count <= 0) {
        if(m_lastDetectCount > 0) {
            m_lastDetectCount = count;
        }
    }

    if(count > 0 && m_lastDetectCount == 0) {
        m_lastDetectCount = count;
        m_detectCount++;
    }

    if (faceInfo != nullptr)
    {
        emit detectCountChanged(QString::number(m_detectCount));
        emit faceCountChanged(QString::number(faceInfo->face_count));
        emit faceYawChanged(QString::number(faceInfo->base_infos->yaw, 'f', 1));
        emit faceRollChanged(QString::number(faceInfo->base_infos->roll, 'f', 1));
        emit facePitchChanged(QString::number(faceInfo->base_infos->pitch, 'f', 1));
        
        emit faceEyeBlinkStatusChanged(faceInfo->base_infos->action & BEF_EYE_BLINK);
        emit facePitchHeadStatusChanged(faceInfo->base_infos->action & BEF_HEAD_PITCH);
        emit faceOpenMouthStatusChanged(faceInfo->base_infos->action & BEF_MOUTH_AH);
        emit facePoutMouthStatusChanged(faceInfo->base_infos->action & BEF_MOUTH_POUT);
        emit faceJumpBrowStatusChanged(faceInfo->base_infos->action & BEF_BROW_JUMP);
        emit faceYawHeadStatusChanged(faceInfo->base_infos->action & BEF_HEAD_YAW);
    }
    else
    {
        emit faceYawChanged(QString(""));
        emit faceRollChanged(QString(""));
        emit facePitchChanged(QString(""));

        emit faceEyeBlinkStatusChanged(false);
        emit facePitchHeadStatusChanged(false);
        emit faceOpenMouthStatusChanged(false);
        emit facePoutMouthStatusChanged(false);
        emit faceJumpBrowStatusChanged(false);
        emit faceYawHeadStatusChanged(false);
    }
}

void BEFaceDetectInfoSynchro::syncFaceVerify(int status, QString imgFile) {
    emit faceVerify(status, imgFile);
}

void BEFaceDetectInfoSynchro::syncFaceVerifyResult(qreal score) {
    QString sScore = QString::number(score, 'f', 2);
    emit faceVerifyResult(sScore, score > SAME_FACE_SCORE ? true : false);
}

void BEFaceDetectInfoSynchro::clearDetectCount() {
    m_detectCount = 0;
    m_lastDetectCount = 0;
}