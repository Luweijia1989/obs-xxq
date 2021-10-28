#include "be_face_attr_info_synchro.h"
BEFaceAttrInfoSynchro::BEFaceAttrInfoSynchro() {
    registerExpType();
}

void BEFaceAttrInfoSynchro::syncFaceAttrInfo(bef_ai_face_attribute_info *faceAttrInfo) {
    if (faceAttrInfo != nullptr)
    {
        auto itr = m_expStrMap.find(faceAttrInfo->exp_type);
        if (itr != m_expStrMap.end())
        {
            emit faceAttrExpressionChanged(itr->second);
        }
        else
        {
            emit faceAttrExpressionChanged(QString(""));
        }

        bool bConfused = false;
        if (faceAttrInfo->confused_prob > 0.3f && faceAttrInfo->confused_prob <= 1.0f) {
            bConfused = true;
        }
        emit faceAttrOtherInfoChanged(false, (int)faceAttrInfo->age, faceAttrInfo->boy_prob > 0.5f? 0:1, 
                                    (int)faceAttrInfo->attractive, (int)faceAttrInfo->happy_score, bConfused);
    }
    else
    {
        emit faceAttrExpressionChanged(QString(""));
        emit faceAttrOtherInfoChanged(true, 0, 0, 0, 0, false);
    }
}

void BEFaceAttrInfoSynchro::registerExpType(){
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_ANGRY, QString("生气")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_DISGUST, QString("厌恶")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_FEAR, QString("害怕")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_HAPPY, QString("高兴")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_SAD, QString("伤心")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_SURPRISE, QString("吃惊")));
     m_expStrMap.insert(std::make_pair(BEF_FACE_ATTRIBUTE_NEUTRAL, QString("平静")));
}