#include <chrono>

#include "be_effect_handle.h"
#include "be_resource_context.h"
#include "be_util.h"
#include "be_defines.h"
#include "bef_effect_ai_version.h"
#include "message/Receiver.h"
#include <new>
#include <QDebug>

bool bef_msg_delegate_manager_callback(void* observer, unsigned int msgId, int arg1, int arg2, const char* arg3);

static int logFuncForEffect(int logLevel, const char* msg)
{
    if (msg != nullptr)
    {
        qDebug() << msg;
    }
    return 0;
}

EffectHandle::EffectHandle() {}

EffectHandle::~EffectHandle() {}

bef_effect_result_t EffectHandle::initializeHandle() {
    bef_effect_result_t ret = 0;
    if (m_initialized)
    {
        return ret;
    }
    m_initialized = true;
    ret = bef_effect_ai_create(&m_renderMangerHandle);
    CHECK_BEF_AI_RET_SUCCESS(ret, "EffectHandle::initializeHandle:: create effect handle failed !");

    ret = bef_effect_ai_check_license(m_renderMangerHandle, getResourceContext()->getFeatureContext()->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS(ret, "EffectHandle::initializeHandle:: check_license failed !");

    ret = bef_effect_ai_init(m_renderMangerHandle, m_init_width, m_init_height, getResourceContext()->getFeatureContext()->getFeatureModelPath(BE_FEATURE_EFFECT).c_str(), "");
    CHECK_BEF_AI_RET_SUCCESS(ret, "EffectHandle::initializeHandle:: init effect handle failed !");

    ret = bef_effect_ai_composer_set_mode(m_renderMangerHandle, 1, 0);
    CHECK_BEF_AI_RET_SUCCESS(ret, "EffectHandle::initializeHandle:: bef_effect_ai_composer_set_mode failed !");

    registerBeautyComposerNodes();

    /*updateComposerNode(TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN, 1, 0.5);
    updateComposerNode(TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN_F, 1, 0.5);*/
    /* setSticker("lizishengdan");
    setFilter("Filter_01_38");*/

    return ret;
}

void EffectHandle::initEffectLog(const std::string& logFile) {
    //bef_effect_result_t ret = bef_effect_ai_set_log_to_local_func(logFuncForEffect);
    //if (ret != 0) {
    //    printf("bef_effect_ai_set_log_to_local_func: %d\n", ret);
    //}
}

void EffectHandle::initEffectMessage(void* receiver) {
    // init msgManager for avatar
    /*bef_render_msg_delegate_manager_init(&m_msgManager);
    if (receiver != nullptr && m_msgManager != nullptr) {
        bef_render_msg_delegate_manager_add(m_msgManager, receiver, bef_msg_delegate_manager_callback);
    }*/
}

bef_effect_result_t EffectHandle::releaseHandle() {
    bef_effect_ai_destroy(m_renderMangerHandle);
    return 0;
}

void EffectHandle::setFlip(bool b)
{
	bef_effect_ai_set_orientation(m_renderMangerHandle, b ? BEF_AI_CLOCKWISE_ROTATE_180 : BEF_AI_CLOCKWISE_ROTATE_0);
}

bef_effect_result_t EffectHandle::process(GLint texture, GLint textureSticker, int width, int height, bool imageMode, int timeStamp) {
    bef_effect_result_t ret = 0;
    m_effectMutex.lock();
    auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    double timestamp = (double)time.count() / 1000.0;

    ScopeTimer t(__FUNCTION__);
    bef_effect_ai_set_width_height(m_renderMangerHandle, width, height);
    auto time_alg_start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    
    if (imageMode) {
        // 图片模式调用
        bef_effect_ai_load_resource_with_timeout(m_renderMangerHandle, -1);
        bef_effect_ai_set_algorithm_force_detect(m_renderMangerHandle, true);
    }
    bef_effect_ai_algorithm_texture(m_renderMangerHandle, texture, timestamp);      
    auto time_render_start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());  
    bef_effect_result_t iRet = bef_effect_ai_process_texture(m_renderMangerHandle, texture, textureSticker, timestamp);
    auto time_render_end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    m_effectMutex.unlock();
    return ret;
}


void EffectHandle::setInitWidthAndHeight(int width, int height) {
    m_init_width = width;
    m_init_height = height;

}

bef_effect_result_t EffectHandle::setEffectWidthAndHeight(int width, int height) {
    bef_effect_ai_set_width_height(m_renderMangerHandle, width, height);
    return 0;
}

int EffectHandle::getComposerNodeCount()
{
    return m_currentMajorPaths.size() + m_currentSubPaths.size();
}

void EffectHandle::updateComposerNode(int subId, int updateStatus, float value) {

    auto itr = m_allComposerNodeMap.find(subId);
    if (itr != m_allComposerNodeMap.end()) {
        BEComposerNode *node = itr->second;
        bool flag = false;
        if (node != nullptr)
        {
            node->setValue(value);
            switch (updateStatus)
            {
            case 0: {
                flag = removeComposerPath(node->isMajor() ? m_currentMajorPaths : m_currentSubPaths, node);
                if (flag)
                {
                    setComposerNodes();
                }
                break;
            }
            case 1:
            case 2:
            {
                flag = addComposerPath(node->isMajor() ? m_currentMajorPaths : m_currentSubPaths, node);
                if (flag)
                {
                    setComposerNodes();
                }
                updateComposerNodeValue(node);
                break;
            }
            default:
                printf("Invalid update operation!\n");
                break;
            }
        }
    }
    else {
        printf("Invalid effect id: %d\n", subId);
    }
}

bool EffectHandle::hasComposerPath(const std::string& nodePath, std::list<std::string>& pathList) {
    for(auto& path : pathList) {
        if(nodePath == path) {
            return true;
        }
    }

    return false;
}

bool EffectHandle::addComposerPath(std::list<std::string> &pathMap, BEComposerNode *node) {
    if (node != nullptr)
    {
        int id = node->isMajor() ? node->getMajorId() : node->getSubId();

        if (!hasComposerPath(node->getNodeName(), pathMap)) {
            pathMap.push_back(node->getNodeName());
            return true;
        }
    }
    return false;
}
bool EffectHandle::removeComposerPath(std::list<std::string> &pathMap, BEComposerNode *node) {
    if (node != nullptr)
    {
        int id = node->isMajor() ? node->getMajorId() : node->getSubId();

        if (hasComposerPath(node->getNodeName(), pathMap)) {
            pathMap.remove(node->getNodeName());
            return true;
        }
    }
    return false;
}

void EffectHandle::setComposerNodes() {
    int majorCount = m_currentMajorPaths.size();
    int subCount = m_currentSubPaths.size();
    char **nodePaths = nullptr;
    if (majorCount + subCount > 0)
    {
        printf("set composerNodes:\n");
        char **nodePaths = (char **)malloc((majorCount + subCount) * sizeof(char *));
        int i = 0;
        for (auto itr = m_currentMajorPaths.begin(); itr != m_currentMajorPaths.end(); itr++)
        {
            nodePaths[i] = (char*) itr->c_str();
            printf("path: %s\n", nodePaths[i]);
            i++;
        }
        for (auto itr = m_currentSubPaths.begin(); itr != m_currentSubPaths.end(); itr++)
        {
            nodePaths[i] = (char*)itr->c_str();
            printf("path: %s\n", nodePaths[i]);
            i++;
        }
        bef_effect_ai_composer_set_nodes(m_renderMangerHandle, (const char**)nodePaths, majorCount + subCount);
        free(nodePaths);
    }
    else
    {
        bef_effect_ai_composer_set_nodes(m_renderMangerHandle, (const char**)nodePaths, 0);
    }
}

void EffectHandle::updateComposerNodeValue(BEComposerNode *node) {
    if (node != nullptr)
    {
        int ret = bef_effect_ai_composer_update_node(m_renderMangerHandle, node->getNodeName().c_str(), node->getKey().c_str(), node->getValue());
        printf("bef_effect_ai_composer_update_node ret %d\n", ret);
    }
}


void EffectHandle::registerBeautyComposerNodes() {
    registerBeautyFaceNodes();
    registerBeautyReshaperNodes();
    registerBeautyStyleMakeup();
}

void EffectHandle::registerComposerNode(int majorId, int subId, bool isMajor, std::string NodeName, std::string key) {
    auto itr = m_allComposerNodeMap.find(subId);
    if (itr == m_allComposerNodeMap.end())
    {
        m_allComposerNodeMap.insert(std::make_pair(subId, new BEComposerNode(majorId, subId, isMajor, NodeName, key)));
    }
}

void EffectHandle::registerBeautyFaceNodes() {

    registerComposerNode(TYPE_BEAUTY_FACE, TYPE_BEAUTY_FACE_SMOOTH, true, NODE_BEAUTY, "smooth");
    registerComposerNode(TYPE_BEAUTY_FACE, TYPE_BEAUTY_FACE_WHITEN, true, NODE_BEAUTY, "whiten");
    registerComposerNode(TYPE_BEAUTY_FACE, TYPE_BEAUTY_FACE_SHARPEN, true, NODE_BEAUTY, "sharp");
}

void EffectHandle::registerBeautyReshaperNodes() {
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_FACE_OVERALL, true, NODE_RESHAPE, "Internal_Deform_Overall");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_FACE_CUT, true, NODE_RESHAPE, "Internal_Deform_CutFace");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_FACE_SMALL, true, NODE_RESHAPE, "Internal_Deform_Face");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_EYE, true, NODE_RESHAPE, "Internal_Deform_Eye");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_EYE_ROTATE, true, NODE_RESHAPE, "Internal_Deform_RotateEye");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_CHEEK, true, NODE_RESHAPE, "Internal_Deform_Zoom_Cheekbone");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_JAW, true, NODE_RESHAPE, "Internal_Deform_Zoom_Jawbone");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_NOSE_LEAN, true, NODE_RESHAPE, "Internal_Deform_Nose");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_NOSE_LONG, true, NODE_RESHAPE, "Internal_Deform_MovNose");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_CHIN, true, NODE_RESHAPE, "Internal_Deform_Chin");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_FOREHEAD, true, NODE_RESHAPE, "Internal_Deform_Forehead");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_MOUTH_ZOOM, true, NODE_RESHAPE, "Internal_Deform_ZoomMouth");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_MOUTH_SMILE, true, NODE_RESHAPE, "Internal_Deform_MouthCorner");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_EYE_SPACING, true, NODE_RESHAPE, "Internal_Eye_Spacing");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_EYE_MOVE, true, NODE_RESHAPE, "Internal_Deform_Eye_Move");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_MOUTH_MOVE, true, NODE_RESHAPE, "Internal_Deform_MovMouth");

    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_BRIGHTEN_EYE, true, NODE_BEAUTY_4ITEMS, "BEF_BEAUTY_BRIGHTEN_EYE");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_REMOVE_POUCH, true, NODE_BEAUTY_4ITEMS, "BEF_BEAUTY_REMOVE_POUCH");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_SMILE_FOLDS, true, NODE_BEAUTY_4ITEMS, "BEF_BEAUTY_SMILES_FOLDS");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_WHITEN_TEETH, true, NODE_BEAUTY_4ITEMS, "BEF_BEAUTY_WHITEN_TEETH");

    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_SINGLE_TO_DOUBLE, true, "double_eye_lid/newmoon", "BEF_BEAUTY_EYE_SINGLE_TO_DOUBLE");
    registerComposerNode(TYPE_BEAUTY_RESHAPE, TYPE_BEAUTY_RESHAPE_EYE_PLUMP, true, "wocan/ziran", "BEF_BEAUTY_EYE_PLUMP");
}

void EffectHandle::registerBeautyStyleMakeup()
{
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_AIDOU, true, "style_makeup/aidou", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_BAIXI, true, "style_makeup/baixi", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_CWEI, true, "style_makeup/cwei", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_DUANMEI, true, "style_makeup/duanmei", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_HANXI, true, "style_makeup/hanxi", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN, true, "style_makeup/nuannan", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_OUMEI, true, "style_makeup/oumei", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_QISE, true, "style_makeup/qise", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_SHENSUI, true, "style_makeup/shensui", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_TIANMEI, true, "style_makeup/tianmei", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_WENNUAN, true, "style_makeup/wennuan", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_YOUYA, true, "style_makeup/youya", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_YUANQI, true, "style_makeup/yuanqi", "Makeup_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_ZHIGAN, true, "style_makeup/zhigan", "Makeup_ALL");

    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_AIDOU_F, true, "style_makeup/aidou", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_BAIXI_F, true, "style_makeup/baixi", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_CWEI_F, true, "style_makeup/cwei", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_DUANMEI_F, true, "style_makeup/duanmei", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_HANXI_F, true, "style_makeup/hanxi", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN_F, true, "style_makeup/nuannan", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_OUMEI_F, true, "style_makeup/oumei", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_QISE_F, true, "style_makeup/qise", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_SHENSUI_F, true, "style_makeup/shensui", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_TIANMEI_F, true, "style_makeup/tianmei", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_WENNUAN_F, true, "style_makeup/wennuan", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_YOUYA_F, true, "style_makeup/youya", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_YUANQI_F, true, "style_makeup/yuanqi", "Filter_ALL");
    registerComposerNode(TYPE_BEAUTY_STYLE_MAKEUP, TYPE_BEAUTY_STYLE_MAKEUP_ZHIGAN_F, true, "style_makeup/zhigan", "Filter_ALL");
}

void EffectHandle::setIntensity(int key, float val) {
    bef_effect_ai_set_intensity(m_renderMangerHandle, key, val);
}

void EffectHandle::setReshapeIntensity(float val1, float val2) {
    bef_effect_ai_update_reshape_face_intensity(m_renderMangerHandle, val1, val2);
}

void EffectHandle::setSticker(const std::string &stickerPath) {
    std::string tempPath = getResourceContext()->getFeatureContext()->getStickerDir() + stickerPath;
    bef_effect_ai_set_effect(m_renderMangerHandle, tempPath.c_str());
}

void EffectHandle::setStickerNoAutoPath(const std::string &stickerPath) {
    bef_effect_ai_set_effect(m_renderMangerHandle, stickerPath.c_str());
}

void EffectHandle::setAnimoji(const std::string& animojiPath) {
    std::string tempPath = getResourceContext()->getFeatureContext()->getAnimojiDir() + animojiPath;
    bef_effect_ai_set_effect(m_renderMangerHandle, tempPath.c_str());
}

void EffectHandle::setAvatar(const std::string& avatarPath) {
    std::string tempPath = getResourceContext()->getFeatureContext()->getAvatarDir() + avatarPath;
    bef_effect_ai_set_effect(m_renderMangerHandle, tempPath.c_str());
}

void EffectHandle::setMiniGame(const std::string &miniGamePath) {
    std::string tempPath = getResourceContext()->getFeatureContext()->getMiniGameDir() + miniGamePath;
    bef_effect_ai_set_effect(m_renderMangerHandle, tempPath.c_str());
}

void EffectHandle::setFilter(const std::string &filterPath) {
    std::string tempPath = getResourceContext()->getFeatureContext()->getFilterDir() + filterPath;
    if (filterPath.length() == 0)
	bef_effect_ai_set_color_filter_v2(m_renderMangerHandle, "");
    else
	bef_effect_ai_set_color_filter_v2(m_renderMangerHandle, tempPath.c_str());
}

// message callback
bool bef_msg_delegate_manager_callback(void* observer, unsigned int msgId, int arg1, int arg2, const char* arg3) {
    Receiver* receiver = static_cast<Receiver*>(observer);
    if (receiver) {
        return receiver->Update(msgId, arg1, arg2, arg3);
    }

    return false;
}

void EffectHandle::addMessageReceiver(void* receiver) {
    /*if (m_msgManager) {
        bef_render_msg_delegate_manager_add(m_msgManager, receiver, (bef_render_msg_delegate_manager_callback)bef_msg_delegate_manager_callback);
    }*/
}

void EffectHandle::sendEffectMessage(unsigned int msgId, int arg1, int arg2, const char* arg3) {
    /*bef_effect_result_t ret = bef_effect_ai_send_msg(m_renderMangerHandle, msgId, arg1, arg2, arg3);
    if (ret != 0) {
        printf("bef_effect_ai_send_msg failed !\n");
    }*/
}

void EffectHandle::setComposerForTest(std::vector<std::string>& nodePath) {
    int nodeSize = nodePath.size();
    char** nodePaths = (char**)malloc((nodeSize) * sizeof(char*));
    
    int i = 0;
    for (auto itr = nodePath.begin(); itr != nodePath.end(); itr++)
    {
        nodePaths[i] = (char*)itr->c_str();
        i++;
    }

    bef_effect_ai_composer_set_nodes(m_renderMangerHandle, (const char**)nodePaths, nodeSize);
    free(nodePaths);
}

void EffectHandle::updateComposerForTest(const std::string& nodeName, const std::string key, float value) {
    int ret = bef_effect_ai_composer_update_node(m_renderMangerHandle, nodeName.c_str(), key.c_str(), value);
    printf("bef_effect_ai_composer_update_node ret %d\n", ret);
}
