#ifndef BE_RESOURCE_CONTEXT_H
#define BE_RESOURCE_CONTEXT_H


#include <string>

#include "effect_manager/be_feature_context.h"

class BEResourceContext {
public:
    BEResourceContext();
    ~BEResourceContext();
    BEFeatureContext *getFeatureContext();    
    void initializeContext();
    void releaseContext();
    void setApplicationDir(const std::string &path);
    std::string getVersion();
private:
    BEFeatureContext *m_featureContext = nullptr;
    std::string m_applicationDir = "";
    bool m_initialized = false;
};

extern BEResourceContext *beResourceContext;
BEResourceContext *getResourceContext();

#endif // !BE_RESOURCE_CONTEXT_H
