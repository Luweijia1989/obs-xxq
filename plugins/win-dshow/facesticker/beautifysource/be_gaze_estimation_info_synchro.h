#ifndef BE_GAZE_ESTIMATION_INFO_SYNCHRO_H
#define BE_GAZE_ESTIMATION_INFO_SYNCHRO_H

#include <QObject>
#include "bef_effect_ai_gaze_estimation.h"


class BEGazeEstimationInfoSynchro : public QObject
{
    Q_OBJECT
public:
    BEGazeEstimationInfoSynchro();
    
    void syncGazeEstimationInfo(bef_ai_gaze_estimation_info* gazeInfo);
signals:
    void gazeEstimationInfoChanged(bool val);
};

#endif
