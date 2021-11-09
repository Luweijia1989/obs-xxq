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
    m_featureContext->setResourceDir(m_applicationDir + "beautyresource/");
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
