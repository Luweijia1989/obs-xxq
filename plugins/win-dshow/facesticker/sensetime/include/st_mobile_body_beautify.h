#ifndef INCLUDE_STMOBILE_ST_MOBILE_BODY_BEAUTIFY_H_
#define INCLUDE_STMOBILE_ST_MOBILE_BODY_BEAUTIFY_H_

#include "st_mobile_common.h"
#include "st_mobile_human_action.h"

/// 该文件中的API不保证线程安全. 多线程调用时, 需要确保安全调用. 例如在create handle没有执行完就执行process可能造成crash; 在process执行过程中调用destroy函数可能会造成crash.

/// @brief 创建美体句柄. Android建议使用st_mobile_body_beautify_create_from_buffer
/// @parma[out] handle 美体句柄, 失败返回NULL
/// @param[in] config 设置创建美体句柄的方式, 处理视频时设置为ST_MOBILE_DETECT_MODE_VIDEO, 处理图片时设置为ST_MOBILE_DETECT_MODE_IMAGE
/// @param[in] model_path 模型文件路径
/// @return 成功返回ST_OK, 失败返回其他错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_authorize();

/// @brief Create body beauty handle. If Android platform, advice to use st_mobile_body_beautify_create_from_buffer
/// @parma[out] handle body beauty handle, if failure, will return NULL
/// @param[in] config handle configuration, If video input, it should be ST_MOBILE_DETECT_MODE_VIDEO. If single picture input, it should be ST_MOBILE_DETECT_MODE_IMAGE
/// @param[in] model_path model file full path
/// @return Success will return ST_OK, otherwise will return error code.Erro code defination refers to st_mobile_common.h, such asST_E_FAIL etc
ST_SDK_API st_result_t
st_mobile_body_beautify_create(
    st_handle_t *handle,
    unsigned int config,
    const char *model_path
);


/// @brief 创建美体句柄
/// @parma[out] handle 美体句柄, 失败返回NULL
/// @param[in] config 设置创建人体行为句柄的方式, 处理视频时设置为ST_MOBILE_DETECT_MODE_VIDEO, 处理图片时设置为ST_MOBILE_DETECT_MODE_IMAGE
/// @param[in] buffer 模型缓存起始地址
/// @param[in] buffer_size 模型缓存大小
/// @return 成功返回ST_OK, 失败返回其他错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_create_from_handle(
st_handle_t *handle,
st_handle_t handle_src,
unsigned int config
);

/// @brief Create body beauty handle with buffer
/// @parma[out] handle body beauty handle, if failure, will return NULL
/// @param[in] config handle configuration, If video input, it should be ST_MOBILE_DETECT_MODE_VIDEO. If single picture input, it should be ST_MOBILE_DETECT_MODE_IMAGE
/// @param[in] buffer model buffer pointer
/// @param[in] buffer_size buffer size
/// @return Success will return ST_OK, otherwise will return error code.Erro code defination refers to st_mobile_common.h, such asST_E_FAIL etc
ST_SDK_API st_result_t
st_mobile_body_beautify_create_from_buffer(
    st_handle_t *handle,
    unsigned int config,
    const unsigned char* buffer,
    unsigned int buffer_size
);

/// @brief create internal GL Context if using graphic buffer internally.
/// @parma[in] handle body beauty handle
/// @return Success will return ST_OK, otherwise will return error code.Erro code defination refers to st_mobile_common.h, such asST_E_FAIL etc
ST_SDK_API st_result_t
st_mobile_body_beautify_create_opengl_context(
    st_handle_t handle
);

/// @brief destroy internal GL Context if using graphic buffer internally.
/// @parma[in] handle body beauty handle
ST_SDK_API void
st_mobile_body_beautify_destroy_opengl_context(
    st_handle_t handle
);

typedef enum {
    ST_BODY_BEAUTIFY_RESULT_SUCCEED,            /// 执行了美体处理
    ST_BODY_BEAUTIFY_RESULT_PARAM_EMPTY,        /// 美体参数太小, 不执行美体
    ST_BODY_BEAUTIFY_RESULT_NO_BODY,            /// 没有检测到肢体, 不执行美体
    ST_BODY_BEAUTIFY_RESULT_BODY_OCCLUDED,      /// 没有足够的检测点, 不执行美体
    ST_BODY_BEAUTIFY_RESULT_NO_CAPABILITY,      /// 没有权限，不执行美体
    ST_BODY_BEAUTIFY_RESULT_OTHER_ERROR         /// 其它错误，不执行美体
} st_body_beautify_result;

/// @brief 对图像buffer进行美体处理, 需要在OpenGL环境中调用
/// @param[in] handle 已初始化的美体句柄
/// @param[in] image_in 用于检测的图像数据
/// @param[in] format_in 用于检测的图像数据的像素格式.
/// @param[in] image_width 用于检测的图像的宽度(以像素为单位)
/// @param[in] image_height 用于检测的图像的高度(以像素为单位)
/// @param[in] image_stride 用于检测的图像的跨度(以像素为单位),即每行的字节数；目前仅支持字节对齐的padding,不支持roi
/// @param[in] rotate 图像中人体的方向
/// @param[in] process_region 输入图像的处理区域，传入有效区域，将只对对应区域做美体处理
/// @param[out] image_out 输出图像数据
/// @param[in] format_out 输出图像格式
/// @param[out] p_result 美体执行结果
/// @return 成功返回ST_OK, 失败返回其他错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_process_buffer(
    st_handle_t handle,
    const unsigned char *image_in,
    st_pixel_format format_in,
    int image_width,
    int image_height,
    int image_stride,
    st_rotate_type rotate,
    st_rect_t* process_region,
    unsigned char *image_out,
    st_pixel_format format_out,
    st_body_beautify_result *p_result
);

/// @brief 对OpenGL纹理进行美体处理, 需要在OpenGL环境中调用. image_in和texture_src的图像必须一致
/// @param[in] handle 已初始化的美体句柄
/// @param[in] texture_src 待处理的纹理id, 仅支持RGBA纹理
/// @param[in] image_in 用于检测的图像数据
/// @param[in] format_in 用于检测的图像数据的像素格式.
/// @param[in] image_width 用于检测的图像和texture_src的宽度(以像素为单位)
/// @param[in] image_height 用于检测的图像和texture_src的高度(以像素为单位)
/// @param[in] image_stride 用于检测的图像的跨度(以像素为单位), 即每行的字节数; 目前仅支持字节对齐的padding, 不支持roi
/// @param[in] rotate 图像中人体的方向
/// @param[in] process_region 输入图像的处理区域，传入有效区域，将只对对应区域做美体处理
/// @param[in] texture_dst 处理后的纹理id, 仅支持RGBA纹理
/// @param[out] image_out 输出图像数据. 为NULL时不输出图像数据
/// @param[in] format_out 输出图像格式
/// @param[out] p_result 美体执行结果
ST_SDK_API st_result_t
st_mobile_body_beautify_process_texture(
    st_handle_t handle,
    unsigned int texture_src,
    const unsigned char *image_in,
    st_pixel_format format_in,
    int image_width,
    int image_height,
    int image_stride,
    st_rotate_type rotate,
    st_rect_t* process_region,
    unsigned int texture_dst,
    unsigned char *image_out,
    st_pixel_format format_out,
    st_body_beautify_result *p_result
);

/// @brief 对OpenGL纹理进行美体处理, 需要在OpenGL环境中调用. 扩展的process_texture接口，支持输入图像buffer（用于检测）与texture的size不同。
/// @param[in] handle 已初始化的美体句柄
/// @param[in] p_in_img 输入图像buffer，内部定义了图像内存地址，宽高，format等信息。目前支持RGBA、RGB、BGR、BGRA、NV21、NV12、YUV格式。
/// @param[in] in_tex 输入OpenGL纹理的id
/// @param[in] tex_width 纹理的宽度
/// @param[in] tex_height 纹理的高度
/// @param[in] rotate 图像中人体的方向（将人体转正需要顺时针旋转的角度）
/// @param[in] texture_dst 处理后的纹理id, 仅支持RGBA纹理
/// @param[out] out_tex 输出OpenGL纹理的id
/// @param[in] p_out_img 输出图像buffer，如果指定的话，需要与输入texture的size相同。目前只支持RGBA格式。
/// @param[out] p_result 美体执行结果，参考st_body_beautify_result的定义
ST_SDK_API st_result_t
st_mobile_body_beautify_process_texture_ex(
    st_handle_t handle,
    const st_multiplane_image_t* p_in_img,
    int in_tex,
    int tex_width, int tex_height,
    st_rotate_type rotate,
    int out_tex,
    st_multiplane_image_t* p_out_img,
    st_body_beautify_result* p_result
);

/// @brief process YUV format image with body beauty effect, output YUV format image result
/// @param[in] handle initialized body beauty handle
/// @param[in] p_in_img input yuv image
/// @param[in] rotate body direction in the picture
/// @param[in] process_region process region, if input is a valid area, body beauty will apply effect only on that area
/// @param[out] p_out_img output yuv image.
/// @param[out] p_result body beauty process result
ST_SDK_API st_result_t
st_mobile_body_beautify_process_yuv_buffer(
    st_handle_t handle,
    const st_multiplane_image_t* p_in_img,
    st_rotate_type rotate,
    st_rect_t* process_region,
    st_multiplane_image_t* p_out_img,
    st_body_beautify_result *p_result
);

ST_SDK_API st_result_t
st_mobile_body_beautify_process_texture_with_yuvbuffer(
    st_handle_t handle,
    unsigned int texture_src,
    const st_multiplane_image_t* p_in_img,
    st_rotate_type rotate,
    st_rect_t* process_region,
    unsigned int texture_dst,
    st_multiplane_image_t* p_out_img,
    st_body_beautify_result *p_result
);

/// @brief 获取肢体检测信息
/// @param[in] handle 已初始化的美体句柄
/// @param[out] p_human_action_ori 原始肢体关键点
/// @param[out] p_human_action 美体处理后的肢体关键点
ST_SDK_API st_result_t
st_mobile_body_beautify_get_body_info(
    st_handle_t handle,
    st_mobile_human_action_t *p_human_action_ori,
    st_mobile_human_action_t *p_human_action
);

/// @brief 获取肢体检测和美体渲染的耗时
/// @param[in] handle 已初始化的美体句柄
/// @param[out] body_detect_time 肢体检测耗时
/// @param[out] render_time 美体渲染耗时
ST_SDK_API st_result_t
st_mobile_body_beautify_get_process_time(
    st_handle_t handle,
    float *body_detect_time,
    float *render_time
);

/// @brief 美体图像输入源
typedef enum {
    ST_BODY_BEAUTIFY_SOURCE_PREVIEW,    /// 相机预览输入
    ST_BODY_BEAUTIFY_SOURCE_VIDEO,      /// 视频文件输入
    ST_BODY_BEAUTIFY_SOURCE_PHOTO       /// 图片文件输入
} st_body_beautify_source;

/// @brief 设置美体输入源
/// @param[in] handle 已初始化的美体句柄
/// @param[in] source 输入源, 为相机预览源, 或者视频/图像源
/// @return 成功返回ST_OK, 错误则返回错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_set_input_source(
    st_handle_t handle,
    st_body_beautify_source source
);

// typedef enum {
//     ST_BODY_BEAUTIFY_REF_PART,  /// 按美体部位的比例调节
//     ST_BODY_BEAUTIFY_REF_HEAD   /// 按头的比例去调节对应美体部位
// } st_body_beautify_ref_type;

// /// @brief 设置美体瘦身方式
// /// @param[in] handle 已初始化的美体句柄
// /// @param[in] type 瘦身方式, 按头的比例, 或瘦身区域比例
// ///     注意, 按不同方式瘦身, 应该设置的参数初始值及范围不同, 参考st_body_beautify_param_type中的美体参数说明
// /// @return 成功返回ST_OK, 错误则返回错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
// ST_SDK_API st_result_t
// st_mobile_body_beautify_set_ref_type(
//     st_handle_t handle,
//     st_body_beautify_ref_type type
// );

typedef enum {
    ST_BODY_BEAUTIFY_PARAM_BODY_LIMIT,          /// 设置检测到的最大肢体数目N(默认值1),持续track已检测到的N个肢体直到肢体数小于N再继续做detect.值越大,检测到的body数目越多,但相应耗时越长. 目前美体仅支持对一个人进行美体
    ST_BODY_BEAUTIFY_PARAM_DETECT_THRESHOLD,    /// 设置肢体检测阈值: TODO
    ST_BODY_BEAUTIFY_PARAM_DETECT_INTERVAL,     /// 设置肢体关键点检测每多少帧进行一次 detect (默认有肢体时30帧detect一次, 无body时10(30/3)帧detect一次). 值越大,cpu占用率越低, 但检测出新body的时间越长.

    /// 以下美体系数默认为0.0，ST_BODY_BEAUTIFY_REF_HEAD模式
    /// 身体整体美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦身，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦身，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_WHOLE_RATIO,
    /// 头部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦头，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦头，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_HEAD_RATIO,
    /// 肩部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦肩，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦肩，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_SHOULDER_RATIO,
    /// 腰部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦腰，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦腰，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_WAIST_RATIO,
    /// 臀部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦臀，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦臀，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_HIP_RATIO,
    /// 腿部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0, INFINITE)，1.0为原始效果，[0, 1)为瘦腿，(1, INFINITE)为变胖
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [-1, 1]，0.0为原始效果，[0, 1]为瘦腿，[-1, 0)为变胖
    ST_BODY_BEAUTIFY_PARAM_LEG_RATIO,
    /// 身体高度按比例调节系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0， 1.0]，0.0为原始效果，(0.0, 1.0]为增高，最高限制为8头身
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [0.0， 1.0]，0.0为原始效果，(0.0, 1.0]为增高，最高限制为8头身
    ST_BODY_BEAUTIFY_PARAM_HEIGHT_RATIO,
    /// 颈部美化系数
    /// ST_BODY_BEAUTIFY_REF_PART模式: [0.0， 1.0]，0.0为原始效果，(0.0, 1.0]为瘦颈
    /// ST_BODY_BEAUTIFY_REF_HEAD模式: [0.0， 1.0]，0.0为原始效果，(0.0, 1.0]为瘦颈
    ST_BODY_BEAUTIFY_PARAM_NECK_RATIO,
    ST_BODY_BEAUTIFY_PARAM_TAKE_PICTURE,

    /// 输入待处理纹理的size发生变化时，是否重置美体效果，大于0为重置美体效果，小于等于0为不重置美体效果。默认为重置美体效果
    ST_BODY_BEAUTIFY_PARAM_RESET_ON_SIZE_CHANGE,
} st_body_beautify_param_type;

/// @brief 设置美体参数
/// @param[in] handle 已初 始化的美体句柄
/// @param[in] type 美体参数类型
/// @param[in] value 参数值
/// @return 成功返回ST_OK, 错误则返回错误码, 错误码定义在st_mobile_common.h中, 如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_set_param(
    st_handle_t handle,
    st_body_beautify_param_type type,
    float value
);

typedef enum
{
    /// 美体输入、输出纹理为RGBA格式
    ST_BODY_BEAUTIFY_TEX_RGBA,
    /// 美体输入、输出纹理为YUV格式（目前只在Android手机上支持）
    ST_BODY_BEAUTIFY_TEX_YUV,
} st_body_beautify_tex_format;

/// @brief 设置美体输出纹理格式
/// @param[in] handle 已初始化的美体句柄
/// @param[in] format 纹理格式，目前支持RGBA与YUV两种格式，其中YUV只在Android手机支持
/// @return 成功返回ST_OK, 错误则返回错误码,错误码定义在st_mobile_common.h中,如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_set_tex_format(
    st_handle_t handle,
    st_body_beautify_tex_format format
);

/// @brief 准备美体渲染需要的资源，必须在OpenGL线程调用！
/// @param[in] handle 已初始化的美体句柄
/// @return 成功返回ST_OK, 错误则返回错误码,错误码定义在st_mobile_common.h中,如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_prepare_gl_resource(
    st_handle_t handle
);

/// @brief 释放美体OpenGL资源. 必须在OpenGL线程中调用
/// @param[in] handle 已初始化的美体句柄
/// @param[in] only_release_texture 是否只释放Texture相关的GL资源，
///             true - 只释放Texture，false - 释放所有GL资源。
///             只释放Texture资源，可以保证下次调用时节省其他资源初始化时间。
ST_SDK_API void
st_mobile_body_beautify_gl_release(
    st_handle_t handle,
    bool only_release_texture
);

/// @brief reset美体句柄。切换处理的视频文件时调用, 以清空缓存数据
/// @param[in] handle 已初始化的美体句柄
ST_SDK_API void
st_mobile_body_beautify_reset(
    st_handle_t handle
);

/// @brief 打开/关闭跳帧检测. 默认关闭
/// @param[in] handle 已初始化的美体句柄
/// @param[in] enable true: 打开跳帧检测, false: 关闭跳帧检测
ST_SDK_API st_result_t
st_mobile_body_beautify_enable_skip_frame_detect(
    st_handle_t handle,
    bool enable
);

/// @brief 打开/关闭渲染, 仅用于能耗测试. 默认打开
/// @param[in] handle 已初始化的美体句柄
/// @param[in] enable true: 打开渲染, false: 关闭渲染
ST_SDK_API void
st_mobile_body_beautify_enable_render(
    st_handle_t handle,
    bool enable
);

/// @brief 返回当前帧内部是否还在进行平滑处理
/// @param[in] handle 已初始化的美体句柄
/// @return true - 当前帧正在平滑；false - 当前帧没有平滑处理
ST_SDK_API bool
st_mobile_body_beautify_is_smoothing(
    st_handle_t handle
);

/// @brief 配置是否在设置美体参数时启用平滑效果
/// @param[in] handle 已初始化的美体句柄
/// @param[in] enable_smooth 设置参数时启用/禁用平滑，默认行为是禁用平滑
/// @return 成功返回ST_OK, 错误则返回错误码,错误码定义在st_mobile_common.h中,如ST_E_FAIL等
ST_SDK_API st_result_t
st_mobile_body_beautify_enable_smooth_on_change_param(st_handle_t handle, bool enable_smooth);

/// @brief 获取当前设置美体参数时是否启用平滑效果
/// @param[in] handle 已初始化的美体句柄
/// @return true - 设置参数时启用平滑，false - 设置参数时禁用平滑
ST_SDK_API bool
st_mobile_body_beautify_is_enable_smooth_on_change_param(st_handle_t handle);

/// @brief 释放美体句柄. 如果没有调用st_mobile_body_beautify_gl_release, 必须在OpenGL线程中调用
/// @param[in] handle 已初始化的美体句柄
ST_SDK_API void
st_mobile_body_beautify_destroy(
    st_handle_t handle
);

#endif  // INCLUDE_STMOBILE_ST_MOBILE_BODY_BEAUTIFY_H_
