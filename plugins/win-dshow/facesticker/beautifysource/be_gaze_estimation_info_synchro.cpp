#include "be_gaze_estimation_info_synchro.h"
#include <cmath>

static const float DEFAULT_POSITION = 0.5f;
static const int DEFAULT_WIDTH = 480;
static const int DEFAULT_HEIGHT = 360;

BEGazeEstimationInfoSynchro::BEGazeEstimationInfoSynchro() {


}

void BEGazeEstimationInfoSynchro::syncGazeEstimationInfo(bef_ai_gaze_estimation_info* gazeInfo) {
	if (gazeInfo == nullptr) return;

    bool bInside = false;
    if (gazeInfo->face_count > 0 && gazeInfo->eye_infos[0].valid) {
        float* lEye = gazeInfo->eye_infos[0].leye_pos;
        float* rEye = gazeInfo->eye_infos[0].reye_pos;
        float* gaze = gazeInfo->eye_infos[0].mid_gaze;
        float eye[3];
        for (int i = 0; i < 3; i++) {
            eye[i] = (lEye[i] + rEye[i]) / 2;
        }
        float x = eye[0] - gaze[0] / gaze[2] * eye[2];
        float y = eye[1] - gaze[1] / gaze[2] * eye[2];


        int width = DEFAULT_WIDTH;
        int height = DEFAULT_HEIGHT;
        float position = DEFAULT_POSITION;
        int leftWidth = width * position;
        int rightWidth = width * (1 - position);
        bInside = (std::fabs(x) <= leftWidth && y <= height && y >= 0);
    }

    emit gazeEstimationInfoChanged(bInside);
}