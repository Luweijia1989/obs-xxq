// Copyright (C) 2019 Beijing Bytedance Network Technology Co., Ltd.
#ifndef EFFECT_SDK_BEF_EFFECT_AI_PUBLIC_DEFINE_H
#define EFFECT_SDK_BEF_EFFECT_AI_PUBLIC_DEFINE_H


#define BEF_INTENSITY_TYPE_NONE                 0
#define BEF_INTENSITY_TYPE_BEAUTY_WHITEN        1   // 美白
#define BEF_INTENSITY_TYPE_BEAUTY_SMOOTH        2   // 磨平
#define BEF_INTENSITY_TYPE_FACE_SHAPE           3   // 大眼/瘦脸
#define BEF_INTENSITY_TYPE_BEAUTY_SHARP         9   // 锐化
#define BEF_INTENSITY_TYPE_GLOBAL_FILTER_V2     12  // 滤镜
#define BEF_INTENSITY_TYPE_BUILDIN_LIP          17  // 唇色
#define BEF_INTENSITY_TYPE_BUILDIN_BLUSHER      18  // 腮红
#ifdef BEF_SDK_API
#undef BEF_SDK_API
#endif

//bef_framework_public_base_define
#ifdef WIN32
    #ifdef __cplusplus
        #ifdef _EFFECT_SDK_EXPORTS_
        #define BEF_SDK_API extern "C" __declspec(dllexport)
        #else
        #define BEF_SDK_API extern "C"
        #endif
    #else
        #define BEF_SDK_API
    #endif
#else
#    ifdef __cplusplus
#        ifdef _EFFECT_SDK_EXPORTS_
#            define BEF_SDK_API extern "C" __attribute__((visibility ("default")))
#        else
#            define BEF_SDK_API extern "C"
#        endif
#    else
#        ifdef _EFFECT_SDK_EXPORTS_
#            define BEF_SDK_API __attribute__((visibility ("default")))
#        else
#            define BEF_SDK_API
#        endif
#    endif
#endif


typedef    short           int16_t;
typedef    int             int32_t;

typedef unsigned long long UINT64;
// def byted effect handle
typedef void *bef_effect_handle_t;

// def byted effect result
typedef int bef_effect_result_t;

typedef int effect_result;

// define bef_intensity_type
typedef int bef_intensity_type;

// start error code
#define BEF_RESULT_SUC                       0  // 成功返回
#define BEF_RESULT_FAIL                     -1  // 内部错误，检查是否初始化，初始化是否成功
#define BEF_RESULT_FILE_NOT_FIND            -2  // 文件没找到，检查代码中的文件路径与实际路径是否匹配

#define BEF_RESULT_INVALID_INTERFACE        -3  // 接口未实现，检查SDK是否具备该功能
#define BEF_RESULT_FILE_OPEN_FAILED         -4  // 文件打开失败，检查文件是否存在，检查对目标文件是否有读写权限

#define BEF_RESULT_INVALID_EFFECT_HANDLE    -5  // 无效的Effect句柄，检查是否初始化，初始化是否成功
#define BEF_RESULT_INVALID_EFFECT_MANAGER   -6  // 无效的EffectManager，检查是否初始化，初始化是否成功
#define BEF_RESULT_INVALID_FEATURE_HANDLE   -7  // 无效的Feature句柄，检查是否初始化，初始化是否成功
#define BEF_RESULT_INVALID_FEATURE          -8  // 无效的Feature，检查是否初始化，初始化是否成功
#define BEF_RESULT_INVALID_RENDER_MANAGER   -9  // 无效的RenderManager，检查是否初始化，初始化是否成功
#define BEF_RESULT_INVALID_ALG_RES          -11 // 无效的算法结果，没有设置美颜等特效可能会有这个报错，大部分情况下可忽略
#define BEF_RESULT_INVALID_ALG_FACE_RES     -12 // 无效的人脸检测结果
#define BEF_RESULT_INVALID_ALG_CAT_FACE_RES -13 // 无效的猫脸检测结果
#define BEF_RESULT_INVALID_ALG_HAND_RES     -14 // 无效的手势检测结果
#define BEF_RESULT_INVALID_ALG_BODY_RES     -15 // 无效的人体检测结果
#define BEF_RESULT_ALG_INIT_FAIL            -17 // 算法初始化失败
#define BEF_RESULT_ALG_FACE_INIT_FAIL       -21 // 人脸检测初始化失败
#define BEF_RESULT_ALG_FACE_106_CREATE_FAIL -22 // 人脸检测106算法创建失败，检查 license 是否有效，检查模型是否有效
#define BEF_RESULT_ALG_FACE_280_CREATE_FAIL -23 // 人脸检测280算法创建失败，检查 license 是否有效，检查模型是否有效
#define BEF_RESULT_ALG_FACE_PREDICT_FAIL    -24 // 人脸检测预测失败，检查传入图像是否正确
#define BEF_RESULT_ALG_EXP_CREATE_FAIL      -25 // 表情检测初始化失败，检查 license 是否有效，检查模型是否有效
#define BEF_RESULT_ALG_HAND_CREATE_FAIL     -26 // 创建手势算法失败，检查 license 是否有效，检查模型是否有效
#define BEF_RESULT_ALG_HAND_PREDICT_FAIL    -27 // 手势算法预测失败，检查传入图像是否正确


#define BEF_RESULT_INVALID_TEXTURE          -36 // 无效的texture，检查传入 texture 是否正确
#define BEF_RESULT_INVALID_IMAGE_DATA       -37 // 无效的图像数据，检查传入 buffer 是否正确
#define BEF_RESULT_INVALID_IMAGE_FORMAT     -38 // 无效的图片格式，检查传入的图像格式是否符合要求
#define BEF_RESULT_INVALID_PARAM_TYPE       -39 // 无效的参数类型，检查传入的参数是否符合要求
#define BEF_RESULT_INVALID_RESOURCE_VERSION -40 // 资源文件指定sdk版本过高，检查资源文件是否正确
#define BEF_RESULT_INVALID_PARAM_VALUE      -47 // 无效的参数值，检查传入的参数是否符合要求

#define BEF_RESULT_SMASH_E_INTERNAL         -101 // 未定义内部错误，请联系技术支持
#define BEF_RESULT_SMASH_E_NOT_INITED       -102 // 未初始化相关资源，请联系技术支持
#define BEF_RESULT_SMASH_E_MALLOC           -103 // 申请内存失败，请联系技术支持
#define BEF_RESULT_SMASH_E_INVALID_PARAM    -104 // 无效的参数，检查传入的参数是否符合要求
#define BEF_RESULT_SMASH_E_ESPRESSO         -105 // ESPRESSO错误，请联系技术支持
#define BEF_RESULT_SMASH_E_MOBILECV         -106 // MOBILECV错误，请联系技术支持
#define BEF_RESULT_SMASH_E_INVALID_CONFIG   -107 // 无效的配置，请检查传入授权文件路径是否正确，授权文件是否受损
#define BEF_RESULT_SMASH_E_INVALID_HANDLE   -108 // 无效的句柄，请检查传入授权文件路径是否正确，授权文件是否受损
#define BEF_RESULT_SMASH_E_INVALID_MODEL    -109 // 无效的模型，请检查传入授权文件路径是否正确，授权文件是否受损
#define BEF_RESULT_SMASH_E_INVALID_PIXEL_FORMAT        -110 // 无效的图像格式，请检查传入图像格式是否在SDK支持的范围内
#define BEF_RESULT_SMASH_E_INVALID_POINT               -111 // 无效的点，请检查传入接口的输入或输出变量是否为null
#define BEF_RESULT_SMASH_E_REQUIRE_FEATURE_NOT_INIT    -112 // 依赖模块没有初始化，请联系技术支持
#define BEF_RESULT_SMASH_E_NOT_IMPL                    -113 // 未实现的接口，请检查调用函数名和参数是否正确

#define BEF_RESULT_INVALID_LICENSE                     -114 // 无效的license，请检查授权文件路径是否正确，模型文件是否受损
#define BEF_RESULT_NULL_BUNDLEID                       -115 // Application/Bundle ID 为空，请检查传入bundle Id或ApplicationId是否为空
#define BEF_RESULT_LICENSE_STATUS_INVALID              -116 // 非法授权文件，请检查授权文件是否正确
#define BEF_RESULT_LICENSE_STATUS_EXPIRED              -117 // 授权文件过期，请检查授权文件是否正确，是否需要更新
#define BEF_RESULT_LICENSE_STATUS_NO_FUNC              -118 // 请求功能不匹配，请检查是否购买对应功能
#define BEF_RESULT_LICENSE_STATUS_ID_NOT_MATCH         -119 // Application/Bundle ID 不匹配，请检查申请授权文件和应用的Application/BundleId是否一致

#define BEF_RESULT_LICENSE_BAG_NULL_PATH               -120 // 授权包路径为空，请检查授权文件路径是否正确
#define BEF_RESULT_LICENSE_BAG_INVALID_PATH            -121 // 错误的授权包路径，请检查传入sdk的路径是否正确
#define BEF_RESULT_LICENSE_BAG_TYPE_NOT_MATCH          -122 // 授权包类型不匹配，请检查授权文件是否正确，是否损坏
#define BEF_RESULT_LICENSE_BAG_INVALID_VERSION         -123 // 无效的版本，请检查是否用了旧版本的授权文件
#define BEF_RESULT_LICENSE_BAG_INVALID_BLOCK_COUNT     -124 // 无效的数据块，请检查授权文件是否正确，是否损坏
#define BEF_RESULT_LICENSE_BAG_INVALID_BLOCK_LEN       -125 // 无效的数据块长度，请检查授权文件是否正确，是否损坏
#define BEF_RESULT_LICENSE_BAG_INCOMPLETE_BLOCK        -126 // 数据块不完整，请检查授权文件是否正确，是否损坏
#define BEF_RESULT_LICENSE_BAG_UNAUTHORIZED_FUNC       -127 // license未授权的功能，请检查是否购买对应功能

#define BEF_RESULT_SDK_FUNC_NOT_INCLUDE                -128 // SDK 未包含功能
#define BEF_RESULT_LICENSE_BAG_INVALID_SUB_FUNC        -129 // 无效的subFunction，证书存在问题，请联系技术支持
#define BEF_RESULT_GL_ERROR_OCCUR                      -150 // opengl发生错误，请检查是否在opengl线程调用接口
#define BEF_RESULT_GL_CONTECT                          -151 // 无效的glcontext，请检查gl上下文是否正确
#define BEF_RESULT_GL_TEXTURE                          -152 // 无效的gltexture，请检查纹理号是否有效
// end error code


/*
 ** 画质增强的错误码都放在这里
 */
#define BEF_RESULT_IMAGE_QUALITY_CREATE_ENGINE_FAILED           -64     // 画质增强引擎创建失败
#define BEF_RESULT_IMAGE_QUALITY_CREATE_VIDEO_SR_FAILED         -65     // 视频超分算法创建失败
#define BEF_RESULT_IMAGE_QUALITY_CREATE_NIGHT_SCENE_FAILED      -66     // 画质算法失败
#define BEF_RESULT_INVALID_HANDLE                               -67     // 无效的画质算法handle

/*
 ** END
 */

#define BEF_EFFECT_FEATURE_LEN                          128 //feature name 默认长度

// bef_framework_public_geometry_define
// @brief image rotate type definition
typedef enum {
    BEF_AI_CLOCKWISE_ROTATE_0 = 0, // 图像不需要旋转，图像中的人脸为正脸
    BEF_AI_CLOCKWISE_ROTATE_90 = 1, // 图像需要顺时针旋转90度，使图像中的人脸为正
    BEF_AI_CLOCKWISE_ROTATE_180 = 2, // 图像需要顺时针旋转180度，使图像中的人脸为正
    BEF_AI_CLOCKWISE_ROTATE_270 = 3  // 图像需要顺时针旋转270度，使图像中的人脸为正
} bef_ai_rotate_type;

// ORDER!!!
typedef enum {
    BEF_AI_PIX_FMT_RGBA8888, // RGBA 8:8:8:8 32bpp ( 4通道32bit RGBA 像素 )
    BEF_AI_PIX_FMT_BGRA8888, // BGRA 8:8:8:8 32bpp ( 4通道32bit RGBA 像素 )
    BEF_AI_PIX_FMT_BGR888,   // BGR 8:8:8 24bpp ( 3通道32bit RGB 像素 )
    BEF_AI_PIX_FMT_RGB888,   // RGB 8:8:8 24bpp ( 3通道32bit RGB 像素 )
    BEF_AI_PIX_FMT_GRAY8,    // GRAY 8bpp ( 1通道8bit 灰度像素 ). 目前还不支持
    BEF_AI_PIX_FMT_YUV420P,  // YUV  4:2:0   12bpp ( 3通道, 一个亮度通道, 另两个为U分量和V分量通道, 所有通道都是连续的 ). 目前还不支持
    BEF_AI_PIX_FMT_NV12,     // YUV  4:2:0   12bpp ( 3通道, 一个亮度通道, 另一道为UV分量交错 ). 目前还不支持
    BEF_AI_PIX_FMT_NV21      // YUV  4:2:0   12bpp ( 3通道, 一个亮度通道, 另一道为VU分量交错 ). 目前还不支持
} bef_ai_pixel_format;


typedef struct bef_ai_fpoint_t {
    float x;
    float y;
} bef_ai_fpoint;

typedef struct bef_ai_fpoint3d_t {
    float x;
    float y;
    float z;
} bef_ai_fpoint3d;

typedef struct bef_ai_rect_t {
    int left;   // Left most coordinate in rectangle. 矩形最左边的坐标
    int top;    // Top coordinate in rectangle.  矩形最上边的坐标
    int right;  // Right most coordinate in rectangle.  矩形最右边的坐标
    int bottom; // Bottom coordinate in rectangle. 矩形最下边的坐标
} bef_ai_rect;

// Same definiation as bef_rect, but in float type
// 和bef_rect一样的定义，类型为单精度浮点
typedef struct bef_ai_rectf_t {
    float left;
    float top;
    float right;
    float bottom;
} bef_ai_rectf;


typedef enum bef_ai_camera_position_t {
    bef_ai_camera_position_front,
    bef_ai_camera_position_back
} bef_ai_camera_position;



typedef struct bef_ai_frect_st {
    float left;   ///< 矩形最左边的坐标
    float top;    ///< 矩形最上边的坐标
    float right;  ///< 矩形最右边的坐标
    float bottom; ///< 矩形最下边的坐标
} bef_ai_frect;


typedef struct bef_ai_image_t {
    const unsigned char *data;
    int width;
    int height;
    int stride;
    int format;
    bef_ai_rotate_type rotate;
} bef_ai_image;

typedef struct bef_ai_tt_key_point_st {
    float x; // 对应 cols, 范围在 [0, width] 之间
    float y; // 对应 rows, 范围在 [0, height] 之间
    bool is_detect; // 如果该值为 false, 则 x,y 无意义
} bef_ai_tt_key_point;

typedef struct {
    const unsigned char* image;          ///< 图像帧数据地址
    bef_ai_pixel_format pixel_fmt;       ///< 图像格式
    int image_width;                     ///< 图像的宽度
    int image_height;                    ///< 图像的高度
    int image_stride;                    ///< 图像的步长(每行的字节数，可能存在padding)
    bef_ai_rotate_type orient;           ///< 图像的方向
} bef_ai_base_args;

typedef enum {
    bef_ai_render_api_gles20 = 0,
    bef_ai_render_api_gles30,
} bef_ai_render_api_type;

typedef enum
{
    BEF_AI_TOUCH_BEGAN,         ///< 触摸开始事件
    BEF_AI_TOUCH_MOVED,         ///< 触摸移动事件, deprecated
    BEF_AI_TOUCH_ENDED,         ///< 触摸结束事件
    BEF_AI_TOUCH_CANCELLED,     ///< 触摸取消事件
    BEF_AI_PAN,                 ///< 滑动手势
    BEF_AI_ROTATE,              ///< 旋转手势
    BEF_AI_SCALE,               ///< 缩放手势
    BEF_AI_LONG_PRESS,          ///< 长按手势
    BEF_AI_DOUBLE_CLICK         ///< 双击手势
} bef_ai_event_code;

typedef char*(*bef_ai_resource_finder)(bef_effect_handle_t, const char *, const char *);


#define EFFECT_HAND_DETECT_DELAY_FRAME_COUNT 4


#endif
