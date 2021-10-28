#ifndef BE_RESOURCE_CONTEXT_H
#define BE_RESOURCE_CONTEXT_H


#include <string>

#include "effect_manager/be_feature_context.h"
#include "be_face_detect_info_synchro.h"
#include "be_face_attr_info_synchro.h"
#include "be_hand_detect_info_synchro.h"
#include "be_light_classify_info_synchro.h"
#include "be_video_cx_info_synchro.h"
#include "be_gaze_estimation_info_synchro.h"
#include "be_petface_info_synchro.h"
#include <QMutex>

class BEResourceContext {
public:
    BEResourceContext();
    ~BEResourceContext();
    BEFeatureContext *getFeatureContext();    
    void initializeContext();
    void releaseContext();
    void setApplicationDir(const std::string &path);

    BEFaceDetectInfoSynchro *getFaceDetectInfoSynchro();
    BEFaceAttrInfoSynchro *getFaceAttrInfoSynchro();
    BEHandDetectInfoSynchro *getHandDetectInfoSynchro();
    BELightClassifyInfoSynchro *getLightClassifyInfoSynchro();
    BEVideoCXInfoSynchro* getVideoCXInfoSynchro();
    BEGazeEstimationInfoSynchro* getGazeEstimationInfoSynchro();
    BEPetFaceInfoSynchro* getPetFaceInfoSynchro();

    void setFaceDetectInfoSynchro(BEFaceDetectInfoSynchro * info);
    void setFaceAttrInfoSynchro(BEFaceAttrInfoSynchro * info);
    void setHandDetectInfoSynchro(BEHandDetectInfoSynchro * info);
    void setLightClassifyInfoSynchro(BELightClassifyInfoSynchro * info);
    void setVideoCXInfoSynchro(BEVideoCXInfoSynchro* info);
    void setGazeEstimationInfoSynchro(BEGazeEstimationInfoSynchro* info);
    void setPetFaceInfoSynchro(BEPetFaceInfoSynchro* info);

    std::string getVersion();
private:
    BEFeatureContext *m_featureContext = nullptr;
    std::string m_applicationDir = "";
    bool m_initialized = false;
    BEFaceDetectInfoSynchro *m_faceDetectInfoSynchro = nullptr; 
    BEFaceAttrInfoSynchro *m_faceAttrInfoSynchro = nullptr;
    BEHandDetectInfoSynchro *m_handDetectInfoSynchro = nullptr;
    BELightClassifyInfoSynchro *m_lightClsInfoSynchro = nullptr;
    BEVideoCXInfoSynchro* m_videoCXInfoSynchro = nullptr;
    BEGazeEstimationInfoSynchro* m_gazeEstimationInfoSynchro = nullptr;
    BEPetFaceInfoSynchro* m_petfaceInfoSynchro = nullptr;
};

extern BEResourceContext *beResourceContext;
BEResourceContext *getResourceContext();

#endif // !BE_RESOURCE_CONTEXT_H
