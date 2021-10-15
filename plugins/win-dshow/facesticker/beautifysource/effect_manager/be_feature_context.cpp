#include "be_feature_context.h"

BEFeatureContext::BEFeatureContext() {

}

BEFeatureContext::~BEFeatureContext() {
    releaeseContext();
}

void BEFeatureContext::initializeContext() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    m_modelDir = m_resourceDir + "model/";
    m_materialDir = m_resourceDir + "material/";
    m_licensePath = m_resourceDir + "license/license.bag";
    m_licenseDir = m_resourceDir + "license/";
    m_stickerDir = m_materialDir + "stickers_new/";
    m_faceCacheDir = m_resourceDir + "faceCache/";
    m_animojiDir = m_materialDir + "animoji/";
    m_avatarDir = m_materialDir + "avatar/";
    m_miniGameDir = m_materialDir + "mini_game/";
    m_filterDir = m_materialDir + "FilterResource.bundle/1/";
    m_comopserMakeupDir = m_materialDir + "ComposeMakeup.bundle/ComposeMakeup/composer/";
    m_composerDir = m_materialDir + "ComposeMakeup.bundle/ComposeMakeup/";
    registerAllModelPath();
}

void BEFeatureContext::releaeseContext() {
    m_feature2ModelMap.clear();
}

std::string BEFeatureContext::getFeatureModelPath(BE_FEATURE beFeat) const {
    std::string model_path = "";

    auto itr = m_feature2ModelMap.find(beFeat);
    if (itr != m_feature2ModelMap.end())
    {
        model_path = itr->second;
    }
    return model_path;
}

const std::string &BEFeatureContext::getLicensePath() const{
    return m_licensePath;
}

const std::string& BEFeatureContext::getLicenseDir() const {
    return m_licenseDir;
}

const std::string& BEFeatureContext::getFaceCacheDir() const {
    return m_faceCacheDir;
}

const std::string &BEFeatureContext::getMaterialDir() const {
    return m_materialDir;
}

const std::string &BEFeatureContext::getStickerDir() const {
    return m_stickerDir;
}

const std::string& BEFeatureContext::getAnimojiDir() const {
    return m_animojiDir;
}

const std::string& BEFeatureContext::getAvatarDir() const {
    return m_avatarDir;
}

const std::string &BEFeatureContext::getFilterDir() const {
    return m_filterDir;
}

const std::string &BEFeatureContext::getMiniGameDir() const {
    return m_miniGameDir;
}


const std::string &BEFeatureContext::getComposerDir() const {
    return m_composerDir;
}

const std::string &BEFeatureContext::getComposerMakeupDir() const {
    return m_comopserMakeupDir;
}

void BEFeatureContext::setResourceDir(const std::string &resDir) {
    m_resourceDir = resDir;
    if (m_resourceDir.length() == 0)
    {
        printf("Error: invalid model dir : %s \n", m_resourceDir.c_str());
        return;
    }
    if (m_resourceDir.rfind("/") != m_resourceDir.length() - 1)
    {
        m_resourceDir.append("/");
    }
}

void BEFeatureContext::registerAllModelPath() {
    if (!m_modelDir.length())
    {
        return;
    }
    
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_EFFECT, m_modelDir));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_FACE, m_modelDir + "ttfacemodel/tt_face_v10.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_FACE_EXTRA, m_modelDir + "ttfacemodel/tt_face_extra_v12.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_FACE_VERIFY, m_modelDir + "faceverifymodel/tt_faceverify_v7.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_FACE_ATTRIBUTE, m_modelDir + "ttfaceattrmodel/tt_face_attribute_v7.0.model"));

    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_SKELETON, m_modelDir + "skeleton_model/tt_skeleton_v7.0.model"));

    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAND_DETECT, m_modelDir + "handmodel/tt_hand_det_v11.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAND_GESTURE, m_modelDir + "handmodel/tt_hand_gesture_v11.1.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAND_SEG, m_modelDir + "handmodel/tt_hand_seg_v2.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAND_KEY_POINT, m_modelDir + "handmodel/tt_hand_kp_v6.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAND_BOX, m_modelDir + "handmodel/tt_hand_box_reg_v12.0.model"));


    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HAIRPARSE, m_modelDir + "hairparser/tt_hair_v10.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_HEAD_SEG, m_modelDir + "headsegmodel/tt_headseg_v6.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_PORTRAIT_MATTING, m_modelDir + "mattingmodel/tt_matting_v13.0.model"));

    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_LIGHT_CLASSIFY, m_modelDir + "lightcls_model/tt_lightcls_v1.0.model"));

    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_C1, m_modelDir + "scenerecognitionmodel/tt_c1_small_v8.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_C2, m_modelDir + "c2clsmodel/tt_C2Cls_v5.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_VIDEO, m_modelDir + "videoclsmodel/tt_videoCls_v4.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_GAZE_ESTIMATION, m_modelDir + "gazeestimationmodel/tt_gaze_v3.0.model"));
    m_feature2ModelMap.insert(std::make_pair(BE_FEATURE_PET_FACE, m_modelDir + "petfacemodel/tt_petface_v3.0.model"));
    
}
