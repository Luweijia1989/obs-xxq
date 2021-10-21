#ifndef BE_EFFECT_HANDLE_H
#define BE_EFFECT_HANDLE_H



#include <windows.h>
#include <mutex>
#include <map>
#include <list>
#include <vector>

#include "bef_effect_ai_api.h"
#include "be_composer_node.h"
#include "gles_loader_autogen.h"
//avatar 交互相关
//#include "bef_msg_delegate_manager.h"

const int OFFSET = 16;
const int SUB_OFFSET = 8;

// 一级菜单
//The second menu

const int TYPE_CLOSE = -1;
// Beautify face 美颜
const int TYPE_BEAUTY_FACE        = 1 << OFFSET;
// Beautify face 美颜四件套
const int TYPE_BEAUTY_FACE_4ITEMS = (1 << OFFSET) +  (1<< SUB_OFFSET);


// Beautify reshape 美型
const int TYPE_BEAUTY_RESHAPE     = 2 << OFFSET;
// Filiter 滤镜
const int TYPE_FILTER             = 5 << OFFSET;
// Makeup option 美妆类型
const int TYPE_MAKEUP_OPTION      = 6 << OFFSET;


const int TYPE_BEAUTY_STYLE_MAKEUP = 11 << OFFSET;

// 二级菜单
//The secondary menu

// Beautify face 美颜
const int TYPE_BEAUTY_FACE_SMOOTH                = TYPE_BEAUTY_FACE + 1;    // 磨皮
const int TYPE_BEAUTY_FACE_WHITEN                = TYPE_BEAUTY_FACE + 2;    // 美白
const int TYPE_BEAUTY_FACE_SHARPEN               = TYPE_BEAUTY_FACE + 3;    // 锐化
//const int TYPE_BEAUTY_FACE_4ITEMS_BRIGHTEN_EYE   = TYPE_BEAUTY_FACE_4ITEMS + 1;    // 亮眼
//const int TYPE_BEAUTY_FACE_4ITEMS_REMOVE_POUCH   = TYPE_BEAUTY_FACE_4ITEMS + 2;    // 黑眼圈
//const int TYPE_BEAUTY_FACE_4ITEMS_SMILE_FOLDS    = TYPE_BEAUTY_FACE_4ITEMS + 3;    // 法令纹
//const int TYPE_BEAUTY_FACE_4ITEMS_WHITEN_TEETH   = TYPE_BEAUTY_FACE_4ITEMS + 4;    // 白牙

// Beautify reshape 美形
const int TYPE_BEAUTY_RESHAPE_FACE_OVERALL = TYPE_BEAUTY_RESHAPE + 1;     // 瘦脸
const int TYPE_BEAUTY_RESHAPE_FACE_CUT     = TYPE_BEAUTY_RESHAPE + 2;     // 窄脸
const int TYPE_BEAUTY_RESHAPE_FACE_SMALL   = TYPE_BEAUTY_RESHAPE + 3;     // 小脸
const int TYPE_BEAUTY_RESHAPE_EYE          = TYPE_BEAUTY_RESHAPE + 4;     // 大眼
const int TYPE_BEAUTY_RESHAPE_EYE_ROTATE   = TYPE_BEAUTY_RESHAPE + 5;     // 眼角度
const int TYPE_BEAUTY_RESHAPE_CHEEK        = TYPE_BEAUTY_RESHAPE + 6;     // 瘦颧骨
const int TYPE_BEAUTY_RESHAPE_JAW          = TYPE_BEAUTY_RESHAPE + 7;     // 下颌骨
const int TYPE_BEAUTY_RESHAPE_NOSE_LEAN    = TYPE_BEAUTY_RESHAPE + 8;     // 瘦鼻
const int TYPE_BEAUTY_RESHAPE_NOSE_LONG    = TYPE_BEAUTY_RESHAPE + 9;     // 长鼻
const int TYPE_BEAUTY_RESHAPE_CHIN         = TYPE_BEAUTY_RESHAPE + 10;    // 下巴
const int TYPE_BEAUTY_RESHAPE_FOREHEAD     = TYPE_BEAUTY_RESHAPE + 11;    // 额头

const int TYPE_BEAUTY_RESHAPE_MOUTH_ZOOM   = TYPE_BEAUTY_RESHAPE + 12;    // 嘴型
const int TYPE_BEAUTY_RESHAPE_MOUTH_SMILE  = TYPE_BEAUTY_RESHAPE + 13;    // 微笑

const int TYPE_BEAUTY_RESHAPE_EYE_SPACING  = TYPE_BEAUTY_RESHAPE + 14;    // 眼距离
const int TYPE_BEAUTY_RESHAPE_EYE_MOVE     = TYPE_BEAUTY_RESHAPE + 15;    // 眼移动
const int TYPE_BEAUTY_RESHAPE_MOUTH_MOVE   = TYPE_BEAUTY_RESHAPE + 16;    // 缩人中

const int TYPE_BEAUTY_RESHAPE_BRIGHTEN_EYE = TYPE_BEAUTY_RESHAPE + 17;    // 亮眼
const int TYPE_BEAUTY_RESHAPE_REMOVE_POUCH = TYPE_BEAUTY_RESHAPE + 18;    // 黑眼圈
const int TYPE_BEAUTY_RESHAPE_SMILE_FOLDS  = TYPE_BEAUTY_RESHAPE + 19;    // 法令纹
const int TYPE_BEAUTY_RESHAPE_WHITEN_TEETH = TYPE_BEAUTY_RESHAPE + 20;    // 白牙
const int TYPE_BEAUTY_RESHAPE_SINGLE_TO_DOUBLE = TYPE_BEAUTY_RESHAPE + 21;    // 双眼皮
const int TYPE_BEAUTY_RESHAPE_EYE_PLUMP = TYPE_BEAUTY_RESHAPE + 22;    // 卧蚕

const int TYPE_BEAUTY_STYLE_MAKEUP_AIDOU   = TYPE_BEAUTY_STYLE_MAKEUP + 1;
const int TYPE_BEAUTY_STYLE_MAKEUP_BAIXI   = TYPE_BEAUTY_STYLE_MAKEUP + 2;
const int TYPE_BEAUTY_STYLE_MAKEUP_CWEI    = TYPE_BEAUTY_STYLE_MAKEUP + 3;
const int TYPE_BEAUTY_STYLE_MAKEUP_DUANMEI = TYPE_BEAUTY_STYLE_MAKEUP + 4;
const int TYPE_BEAUTY_STYLE_MAKEUP_HANXI   = TYPE_BEAUTY_STYLE_MAKEUP + 5;
const int TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN = TYPE_BEAUTY_STYLE_MAKEUP + 6;
const int TYPE_BEAUTY_STYLE_MAKEUP_OUMEI   = TYPE_BEAUTY_STYLE_MAKEUP + 7;
const int TYPE_BEAUTY_STYLE_MAKEUP_QISE    = TYPE_BEAUTY_STYLE_MAKEUP + 8;
const int TYPE_BEAUTY_STYLE_MAKEUP_SHENSUI = TYPE_BEAUTY_STYLE_MAKEUP + 9;
const int TYPE_BEAUTY_STYLE_MAKEUP_TIANMEI = TYPE_BEAUTY_STYLE_MAKEUP + 10;
const int TYPE_BEAUTY_STYLE_MAKEUP_WENNUAN = TYPE_BEAUTY_STYLE_MAKEUP + 11;
const int TYPE_BEAUTY_STYLE_MAKEUP_YOUYA   = TYPE_BEAUTY_STYLE_MAKEUP + 12;
const int TYPE_BEAUTY_STYLE_MAKEUP_YUANQI  = TYPE_BEAUTY_STYLE_MAKEUP + 13;
const int TYPE_BEAUTY_STYLE_MAKEUP_ZHIGAN  = TYPE_BEAUTY_STYLE_MAKEUP + 14;

const int TYPE_BEAUTY_STYLE_MAKEUP_AIDOU_F   = TYPE_BEAUTY_STYLE_MAKEUP + 15;
const int TYPE_BEAUTY_STYLE_MAKEUP_BAIXI_F   = TYPE_BEAUTY_STYLE_MAKEUP + 16;
const int TYPE_BEAUTY_STYLE_MAKEUP_CWEI_F    = TYPE_BEAUTY_STYLE_MAKEUP + 17;
const int TYPE_BEAUTY_STYLE_MAKEUP_DUANMEI_F = TYPE_BEAUTY_STYLE_MAKEUP + 18;
const int TYPE_BEAUTY_STYLE_MAKEUP_HANXI_F   = TYPE_BEAUTY_STYLE_MAKEUP + 19;
const int TYPE_BEAUTY_STYLE_MAKEUP_NUANNAN_F = TYPE_BEAUTY_STYLE_MAKEUP + 20;
const int TYPE_BEAUTY_STYLE_MAKEUP_OUMEI_F   = TYPE_BEAUTY_STYLE_MAKEUP + 21;
const int TYPE_BEAUTY_STYLE_MAKEUP_QISE_F    = TYPE_BEAUTY_STYLE_MAKEUP + 22;
const int TYPE_BEAUTY_STYLE_MAKEUP_SHENSUI_F = TYPE_BEAUTY_STYLE_MAKEUP + 23;
const int TYPE_BEAUTY_STYLE_MAKEUP_TIANMEI_F = TYPE_BEAUTY_STYLE_MAKEUP + 24;
const int TYPE_BEAUTY_STYLE_MAKEUP_WENNUAN_F = TYPE_BEAUTY_STYLE_MAKEUP + 25;
const int TYPE_BEAUTY_STYLE_MAKEUP_YOUYA_F   = TYPE_BEAUTY_STYLE_MAKEUP + 26;
const int TYPE_BEAUTY_STYLE_MAKEUP_YUANQI_F  = TYPE_BEAUTY_STYLE_MAKEUP + 27;
const int TYPE_BEAUTY_STYLE_MAKEUP_ZHIGAN_F  = TYPE_BEAUTY_STYLE_MAKEUP + 28;


// Node name 结点名称
const std::string NODE_BEAUTY = "beauty_Android_live";
const std::string NODE_BEAUTY_4ITEMS = "beauty_4Items";
const std::string NODE_RESHAPE = "reshape_live";


typedef enum
{
    EFFECT_TYPE_BEAUTY = 0,
    EFFECT_TYPE_RESHAPE = 1,
    EFFECT_TYPE_MAKEUP = 2,
}BE_EFFECT_TYPE;

class EffectHandle {
public:
	void test();
    EffectHandle();
    ~EffectHandle();
    bef_effect_result_t initializeHandle();
    void initEffectLog(const std::string& logFile);
    void initEffectMessage(void* receiver);
    bef_effect_result_t releaseHandle();
    bef_effect_result_t process(GLint texture, GLint textureSticker, int width, int height, bool imageMode, int timeStamp);
    void setInitWidthAndHeight(int width, int height);
    bef_effect_result_t setEffectWidthAndHeight(int width, int height);
public:
    typedef std::map<int, BEComposerNode*> ComposerNodeMap;
    //updateStatus 0: off , 1: on, 2: update value
    void updateComposerNode(int subId, int updateStatus, float value);
    int getComposerNodeCount();
    
    // 性能测试接口
    void setComposerForTest(std::vector<std::string>& nodePath);
    void updateComposerForTest(const std::string& nodeName, const std::string key, float value);
private:
    void setComposerNodes();
    void updateComposerNodeValue(BEComposerNode *node);


    bool hasComposerPath(const std::string& node, std::list<std::string>& pathList);
    bool addComposerPath(std::list<std::string> &pathMap, BEComposerNode *node);
    bool removeComposerPath(std::list<std::string> &pathMap, BEComposerNode *node);

private:
    void registerBeautyComposerNodes();
    void registerBeautyFaceNodes();
    void registerBeautyReshaperNodes();
    void registerBeautyStyleMakeup();
    void registerComposerNode(int majorId,int subId, bool isMajor, std::string NodeName, std::string key);
public:
    void setIntensity(int key, float val);
    void setReshapeIntensity(float val1, float val2);
    void setSticker(const std::string &stickerPath);
    void setStickerNoAutoPath(const std::string &stickerPath);
    void setAnimoji(const std::string& animojiPath);
    void setAvatar(const std::string& avatarPath);
    void setMiniGame(const std::string &miniGamePath);
    void setFilter(const std::string &filterPath);
    void addMessageReceiver(void *receiver);
    void sendEffectMessage(unsigned int msgId, int arg1, int arg2, const char* arg3);

private:
    bef_effect_handle_t m_renderMangerHandle = 0;
    int m_init_width = 0;
    int m_init_height = 0;
    std::mutex m_effectMutex;
    bool m_initialized = false;
    //bef_render_msg_delegate_manager m_msgManager = nullptr;
    ComposerNodeMap m_allComposerNodeMap;

    std::list<std::string> m_currentMajorPaths;
    std::list<std::string> m_currentSubPaths;

    ComposerNodeMap m_currentComposerNodes;
};

#endif // BE_EFFECT_HANDLE_H
