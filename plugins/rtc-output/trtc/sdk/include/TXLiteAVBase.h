/**
 * Module:   TXLiteAVBase @ liteav
 *
 * Function: SDK 公共定义头文件
 *
 */

#ifndef __TXLITEAVBASE_H__
#define __TXLITEAVBASE_H__

#ifdef _WIN32
//防止windows用户引用TXLiteAVBase.h报错
#include "TRTCTypeDef.h"
#endif

#ifdef LITEAV_EXPORTS
#define LITEAV_API __declspec(dllexport)
#else
#define LITEAV_API __declspec(dllimport)
#endif

extern "C" {
    /// @name SDK 导出基础功能接口
    /// @{
    /**
     * \brief 获取 SDK 版本号
     *
     * \return 返回 UTF-8 编码的版本号。
     */
    LITEAV_API const char* getLiteAvSDKVersion();
    /// @}
}

/**
 * 以下定义仅用于兼容原有接口，具体定义参见 TRTCTypeDef.h 文件
 */
typedef TRTCVideoBufferType LiteAVVideoBufferType;
#define LiteAVVideoBufferType_Unknown TRTCVideoBufferType_Unknown
#define LiteAVVideoBufferType_Buffer  TRTCVideoBufferType_Buffer
#define LiteAVVideoBufferType_Texture TRTCVideoBufferType_Texture

typedef TRTCVideoPixelFormat LiteAVVideoPixelFormat;
#define LiteAVVideoPixelFormat_Unknown    TRTCVideoPixelFormat_Unknown
#define LiteAVVideoPixelFormat_I420       TRTCVideoPixelFormat_I420
#define LiteAVVideoPixelFormat_Texture_2D TRTCVideoPixelFormat_Texture_2D
#define LiteAVVideoPixelFormat_BGRA32     TRTCVideoPixelFormat_BGRA32

typedef TRTCAudioFrameFormat LiteAVAudioFrameFormat;
#define LiteAVAudioFrameFormatNone TRTCAudioFrameFormatNone
#define LiteAVAudioFrameFormatPCM  TRTCAudioFrameFormatPCM

typedef TRTCVideoRotation LiteAVVideoRotation;
#define LiteAVVideoRotation0   TRTCVideoRotation0
#define LiteAVVideoRotation90  TRTCVideoRotation90
#define LiteAVVideoRotation180 TRTCVideoRotation180
#define LiteAVVideoRotation270 TRTCVideoRotation270

typedef TRTCVideoFrame LiteAVVideoFrame;
typedef TRTCAudioFrame LiteAVAudioFrame;

typedef TRTCScreenCaptureSourceType LiteAVScreenCaptureSourceType;
#define LiteAVScreenCaptureSourceTypeUnknown TRTCScreenCaptureSourceTypeUnknown
#define LiteAVScreenCaptureSourceTypeWindow  TRTCScreenCaptureSourceTypeWindow
#define LiteAVScreenCaptureSourceTypeScreen  TRTCScreenCaptureSourceTypeScreen
#define LiteAVScreenCaptureSourceTypeCustom  TRTCScreenCaptureSourceTypeCustom

typedef TRTCImageBuffer LiteAVImageBuffer;
typedef TRTCScreenCaptureSourceInfo LiteAVScreenCaptureSourceInfo;
typedef ITRTCScreenCaptureSourceList ILiteAVScreenCaptureSourceList;
typedef TRTCScreenCaptureProperty LiteAVScreenCaptureProperty;

typedef ITRTCDeviceInfo ILiteAVDeviceInfo;
typedef ITRTCDeviceCollection ILiteAVDeviceCollection;

class ILiteAVStreamDataSource
{
public:
    /**
    * \brief SDK在成功请求到视频位后会调用该方法以通知数据源开始工作
    */
    virtual void onStart() = 0;

    /**
    * \brief SDK在不再需要用到该数据源的时候会调用该方法以通知数据源停止工作
    */
    virtual void onStop() = 0;

    /**
    * \brief SDK在需要视频帧时调用该方法以请求视频帧
    *
    * \param frame 用于存放请求到的视频帧，其中
    *                   bufferType      无效，暂时只支持LiteAVVideoBufferType_Buffer类型
    *                   videoFormat     必填
    *                   data            SDK已创建好buffer，数据源仅负责将视频数据拷贝其中
    *                   textureId       无效
    *                   length          必填，初始值指示data字段可用空间大小，需填写为可用数据大小
    *                   width           必填
    *                   height          必填
    *                   timestamp       可选
    *                   rotation        可选
    *
    * \return 可用数据大小，<0 表示出错
    */
    virtual int onRequestVideoFrame(LiteAVVideoFrame &frame) = 0;

    /**
    * \brief SDK在需要视频帧时调用该方法以请求音频帧
    *
    * \param frame 用于存放请求到的视频帧，其中
    *                   audioFormat     无效，暂时只支持LiteAVAudioFrameFormatPCM类型
    *                   data            SDK已创建好buffer，数据源仅负责将视频数据拷贝其中
    *                   length          必填，初始值指示data字段可用空间大小，需填写为可用数据大小
    *                   sampleRate      必填
    *                   channel         必填
    *                   timestamp       可选
    *
    * \return 可用数据大小，<0 表示出错
    */
    virtual int onRequestAudioFrame(LiteAVAudioFrame &frame) = 0;

public:
    typedef void(*OnDestoryCallback)(void*);

    ILiteAVStreamDataSource()
    {
        m_hOnDestoryCallbackMutex = NULL;
        m_pOnDestoryCallback = NULL;
        m_pOnDestoryCallbackParam = NULL;
        m_hOnDestoryCallbackMutex = CreateMutex(NULL, FALSE, NULL);
    }

    virtual ~ILiteAVStreamDataSource()
    {
        WaitForSingleObject(m_hOnDestoryCallbackMutex, INFINITE);
        if (m_pOnDestoryCallback)
        {
            m_pOnDestoryCallback(m_pOnDestoryCallbackParam);
        }
        ReleaseMutex(m_hOnDestoryCallbackMutex);
        CloseHandle(m_hOnDestoryCallbackMutex);
    }

    void setOnDestoryCallback(OnDestoryCallback callback, void* param)
    {
        WaitForSingleObject(m_hOnDestoryCallbackMutex, INFINITE);
        m_pOnDestoryCallback = callback;
        m_pOnDestoryCallbackParam = param;
        ReleaseMutex(m_hOnDestoryCallbackMutex);
    }
private:
    HANDLE m_hOnDestoryCallbackMutex;
    OnDestoryCallback m_pOnDestoryCallback;
    void *m_pOnDestoryCallbackParam;

};

#endif /* __TXLITEAVBASE_H__ */
