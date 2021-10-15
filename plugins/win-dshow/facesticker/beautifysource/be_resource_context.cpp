#include "be_resource_context.h"
#include "bef_effect_ai_version.h"

BEResourceContext *beResourceContext = nullptr;
BEResourceContext *getResourceContext() {
    return beResourceContext;
}

BEResourceContext::BEResourceContext() {

}

BEResourceContext::~BEResourceContext() {
    releaseContext();
}

BEFeatureContext *BEResourceContext::getFeatureContext() {
    return m_featureContext;
}

std::string BEResourceContext::getVersion() {   
    std::string str;
    str.resize(10);
    bef_effect_ai_get_version((char*)str.c_str(), 10);
    return str;
}

void BEResourceContext::initializeContext() {
    if (m_initialized)
    {
        return;
    }
    m_featureContext = new BEFeatureContext();
    m_initialized = true;
    m_featureContext->setResourceDir(m_applicationDir + "resource/");
    m_featureContext->initializeContext();
}

void BEResourceContext::releaseContext() {
    if (m_featureContext != nullptr)
    {
        m_featureContext->releaeseContext();
        delete m_featureContext;
        m_featureContext = nullptr;
    }
}

void BEResourceContext::setApplicationDir(const std::string &path) {
    m_applicationDir = path;
}


BEFaceDetectInfoSynchro *BEResourceContext::getFaceDetectInfoSynchro() { 
    return m_faceDetectInfoSynchro;
}

BEFaceAttrInfoSynchro *BEResourceContext::getFaceAttrInfoSynchro() { 
    return m_faceAttrInfoSynchro; 
}

BEHandDetectInfoSynchro *BEResourceContext::getHandDetectInfoSynchro() {
    return m_handDetectInfoSynchro;
}

BELightClassifyInfoSynchro *BEResourceContext::getLightClassifyInfoSynchro() {
    return m_lightClsInfoSynchro;
}

BEVideoCXInfoSynchro* BEResourceContext::getVideoCXInfoSynchro() {
    return m_videoCXInfoSynchro;
}

BEGazeEstimationInfoSynchro* BEResourceContext::getGazeEstimationInfoSynchro() {
    return m_gazeEstimationInfoSynchro;
}

BEPetFaceInfoSynchro* BEResourceContext::getPetFaceInfoSynchro() {
    return m_petfaceInfoSynchro;
}

void BEResourceContext::setFaceDetectInfoSynchro(BEFaceDetectInfoSynchro* info) {
    m_faceDetectInfoSynchro = info;
}

void BEResourceContext::setFaceAttrInfoSynchro(BEFaceAttrInfoSynchro * info) {
    m_faceAttrInfoSynchro = info;
}

void BEResourceContext::setHandDetectInfoSynchro(BEHandDetectInfoSynchro * info) {
    m_handDetectInfoSynchro = info;
}

void BEResourceContext::setLightClassifyInfoSynchro(BELightClassifyInfoSynchro * info) {
    m_lightClsInfoSynchro = info;
}

void BEResourceContext::setVideoCXInfoSynchro(BEVideoCXInfoSynchro* info) {
    m_videoCXInfoSynchro = info;
}

void BEResourceContext::setGazeEstimationInfoSynchro(BEGazeEstimationInfoSynchro* info) {
    m_gazeEstimationInfoSynchro = info;
}

void BEResourceContext::setPetFaceInfoSynchro(BEPetFaceInfoSynchro* info) {
    m_petfaceInfoSynchro = info;
}

