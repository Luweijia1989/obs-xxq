#include "be_light_classify_info_synchro.h"


BELightClassifyInfoSynchro::BELightClassifyInfoSynchro() {
    registerLightClsType();
}

void BELightClassifyInfoSynchro::syncLightClassifyInfo(bef_ai_light_cls_result* lightClsInfo) {
    if (lightClsInfo != nullptr)
    {
        emit lightClassifyTypeChanged(QString::fromLocal8Bit(lightClsInfo->name.c_str()));
        emit lightClassifyProbChanged(QString::number(lightClsInfo->prob, 'f', 5));
    }
    else
    {
        emit lightClassifyTypeChanged(QString(""));
        emit lightClassifyProbChanged(QString(""));
    }
}

void BELightClassifyInfoSynchro::registerLightClsType() {

}

