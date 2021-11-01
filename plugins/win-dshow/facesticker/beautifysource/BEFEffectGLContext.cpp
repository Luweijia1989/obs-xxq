//
// Created by brillian.ni on 2019/12/25.
//

#include "BEFEffectGLContext.h"
#include <iostream>
#if USE_DYNAMIC_ANGLE
#include "egl_loader_autogen.h"
#elif BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
#include <emscripten/html5.h>
#else
#include <EGL/egl.h>
#endif
#include <vector>


#if defined(BEF_EFFECT_PLATFORM_WINDOWS)
struct EGLPlatformParameters
{
    EGLint renderer = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    EGLint majorVersion = EGL_DONT_CARE;
    EGLint minorVersion = EGL_DONT_CARE;
    EGLint deviceType = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    EGLint presentPath = EGL_DONT_CARE;
    EGLint debugLayersEnabled = EGL_DONT_CARE;
    EGLint contextVirtualization = EGL_DONT_CARE;
    EGLint commandGraphFeature = EGL_DONT_CARE;
    EGLint transformFeedbackFeature = EGL_DONT_CARE;
};
#endif

#define LOGS(...) printf(__VA_ARGS__);printf("\n");

NAMESPACE_BEF_EFFECT_FRAMEWORK_BEGIN

struct BEFEffectGLContext::EffectGLEnv {
    bool initialized = false;
    int glesVersion = 2;
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_LINUX || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_ANDROID
    EGLConfig eglConfig = nullptr;
    EGLSurface eglSurface = nullptr;
    EGLContext eglContext = nullptr;
    EGLDisplay eglDisplay = nullptr;
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS
    BEFWindow hWnd = NULL;
#else
    EGLNativeWindowType hWnd = NULL; //this sample use by web, disable linux
#endif
    int eglTextureWidth = 1;
    int eglTextureHeight = 1;
#if USE_D3D
    //create surface shared with D3D
    void* eglSharedHandle = nullptr;
#endif
#elif BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
#endif
};

BEFEffectGLContext::BEFEffectGLContext() {
    m_effectGLEnv = new EffectGLEnv();
    m_bUsePbo = true;
}

BEFEffectGLContext::~BEFEffectGLContext() {
    if (m_effectGLEnv->initialized) {
        releaseGLContext();
    }
    if (m_effectGLEnv) {
        delete m_effectGLEnv;
    }
}

#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_LINUX || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_ANDROID
bool BEFEffectGLContext::createGLESContext(int major) {
    // EGL config attributes
    EGLint renderType = EGL_OPENGL_ES2_BIT;
    EGLint clientVersion = 2;
    EGLint numConfigs;
    m_effectGLEnv->glesVersion = 2;
#ifdef EGL_OPENGL_ES3_BIT
    if (major >= 3) {
        renderType = EGL_OPENGL_ES3_BIT;
        clientVersion = 3;
        m_effectGLEnv->glesVersion = 3;
    }
#endif
    // surface attributes
    // the surface size is set to the input frame size
    const EGLint surfaceAttr_screen[] = {
            EGL_NONE
    };
    const EGLint surfaceAttr_offscreen[] = {
            EGL_WIDTH, m_effectGLEnv->eglTextureWidth,
            EGL_HEIGHT, m_effectGLEnv->eglTextureHeight,
            EGL_NONE
    };
    // EGL context attributes
    std::vector<EGLint> ctxAttr;

    ctxAttr.push_back(EGL_CONTEXT_CLIENT_VERSION);
    ctxAttr.push_back(clientVersion);
    const EGLint confAttr_screen[] = {
            EGL_RENDERABLE_TYPE, renderType,// very important!
            EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,//EGL_WINDOW_BIT EGL_PBUFFER_BIT we will create a pixelbuffer surface
            EGL_BUFFER_SIZE, 0,
            EGL_RED_SIZE, 5,
            EGL_GREEN_SIZE, 6,
            EGL_BLUE_SIZE, 5,
            EGL_ALPHA_SIZE, 0,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
            EGL_DEPTH_SIZE, 24,
            EGL_LEVEL, 0,
            EGL_SAMPLE_BUFFERS, 0,
            EGL_SAMPLES, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_TRANSPARENT_TYPE, EGL_NONE,
            EGL_TRANSPARENT_RED_VALUE, EGL_DONT_CARE,
            EGL_TRANSPARENT_GREEN_VALUE, EGL_DONT_CARE,
            EGL_TRANSPARENT_BLUE_VALUE, EGL_DONT_CARE,
            EGL_CONFIG_CAVEAT, EGL_DONT_CARE,
            EGL_CONFIG_ID, EGL_DONT_CARE,
            EGL_MAX_SWAP_INTERVAL, EGL_DONT_CARE,
            EGL_MIN_SWAP_INTERVAL, EGL_DONT_CARE,
            EGL_NATIVE_RENDERABLE, EGL_DONT_CARE,
            EGL_NATIVE_VISUAL_TYPE, EGL_DONT_CARE,
            EGL_NONE
    };
    const EGLint confAttr_offscreen[] = {
            EGL_RENDERABLE_TYPE, renderType,// very important!
            EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,//EGL_WINDOW_BIT EGL_PBUFFER_BIT we will create a pixelbuffer surface
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_ALPHA_SIZE, 8,// if you need the alpha channel
            EGL_DEPTH_SIZE, 8,// if you need the depth buffer
            EGL_STENCIL_SIZE,8,
            EGL_NONE
    };
    const EGLint* confAttr = confAttr_offscreen;
    if (m_effectGLEnv->hWnd != NULL) {
        confAttr = confAttr_screen;
    }
    // choose the first config, i.e. best config
    if (!eglChooseConfig(m_effectGLEnv->eglDisplay, confAttr, &m_effectGLEnv->eglConfig, 1, &numConfigs))
    {
        LOGS("some config is wrong");
        return false;
    }
    else
    {
        LOGS("all configs is OK");
    }
#if defined(__EMSCRIPTEN__)
    EGLNativeWindowType dummyWindow;
    m_effectGLEnv->eglSurface = eglCreateWindowSurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglConfig, dummyWindow, surfaceAttr_screen);
#else
    if (m_effectGLEnv->hWnd != NULL) {
        m_effectGLEnv->eglSurface = eglCreateWindowSurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglConfig, static_cast<EGLNativeWindowType>(m_effectGLEnv->hWnd), surfaceAttr_screen);
    }
    else {
#if USE_D3D
        if (m_effectGLEnv->eglSharedHandle != NULL)
            m_effectGLEnv->eglSurface = eglCreatePbufferFromClientBuffer(m_effectGLEnv->eglDisplay, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, m_effectGLEnv->eglSharedHandle, m_effectGLEnv->eglConfig, surfaceAttr_offscreen);
        else
#endif
            // create a pixelbuffer surface
            m_effectGLEnv->eglSurface = eglCreatePbufferSurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglConfig, surfaceAttr_offscreen);
    }
#endif

    if (m_effectGLEnv->eglSurface == EGL_NO_SURFACE)
    {
        EGLint error = eglGetError();
        switch (error)
        {
        case EGL_BAD_ALLOC:
            // Not enough resources available. Handle and recover
            LOGS("Not enough resources available");
            break;
        case EGL_BAD_CONFIG:
            // Verify that provided EGLConfig is valid
            LOGS("provided EGLConfig is invalid");
            break;
        case EGL_BAD_PARAMETER:
            // Verify that the EGL_WIDTH and EGL_HEIGHT are
            // non-negative values
            LOGS("provided EGL_WIDTH and EGL_HEIGHT is invalid");
            break;
        case EGL_BAD_MATCH:
            // Check window and EGLConfig attributes to determine
            // compatibility and pbuffer-texture parameters
            LOGS("Check window and EGLConfig attributes");
            break;
        }
        LOGS("create surface failed");
        return false;
    }
    const char* displayExtensions = eglQueryString(m_effectGLEnv->eglDisplay, EGL_EXTENSIONS);
    bool hasProgramCacheControlExtention = strstr(displayExtensions, "EGL_ANGLE_program_cache_control") != nullptr;

    if (hasProgramCacheControlExtention)
    {
        ctxAttr.push_back(EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE);
        ctxAttr.push_back(EGL_TRUE);
        eglProgramCacheResizeANGLE(m_effectGLEnv->eglDisplay, 10 * 1024 * 1024, EGL_PROGRAM_CACHE_RESIZE_ANGLE);
        //isSupportProgramCache = true;
    }

    ctxAttr.push_back(EGL_NONE);

    m_effectGLEnv->eglContext = eglCreateContext(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglConfig, EGL_NO_CONTEXT, ctxAttr.data());

    //m_effectGLEnv->eglContext = eglCreateContext(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglConfig, EGL_NO_CONTEXT, ctxAttr);
    if (m_effectGLEnv->eglContext == EGL_NO_CONTEXT)
    {
        EGLint error = eglGetError();
        if (error == EGL_BAD_CONFIG)
        { 
            // Handle error and recover
            LOGS("EGL_BAD_CONFIG");
        }
        eglDestroySurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface);
        m_effectGLEnv->eglSurface = EGL_NO_SURFACE;
        return false;
    }
    return true;
}
#endif

int BEFEffectGLContext::getGLESVersion() {
    if (m_effectGLEnv) {
        return m_effectGLEnv->glesVersion;
    }
    return -1;
}

#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS
bool BEFEffectGLContext::initGLContext(BEFWindow hWnd) {
    m_effectGLEnv->hWnd = hWnd;
    return initGLContext();
}
#if USE_D3D
bool BEFEffectGLContext::initGLContextSharedWithD3D(void* sharedHandle, int width, int height) {
    if (m_effectGLEnv->initialized) {
        LOGS("u already have glContext, u need releaseGLContext before initGLContext");
        return false;
    }

    m_effectGLEnv->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    m_effectGLEnv->eglSharedHandle = sharedHandle;
    m_effectGLEnv->eglTextureWidth = width;
    m_effectGLEnv->eglTextureHeight = height;

    initGLContext();
}
#endif
#elif BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
bool BEFEffectGLContext::initGLContext(const char* canvas) {
    if (m_effectGLEnv->initialized) {
        LOGS("u already have glContext, u need releaseGLContext before initGLContext");
        return false;
    }
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    // attr.explicitSwapControl = EM_TRUE;
    attr.majorVersion = 2;
    attr.minorVersion = 0;
    m_effectGLEnv->glesVersion = 3;
    m_effectGLEnv->ctx = emscripten_webgl_create_context(canvas, &attr);
    if (m_effectGLEnv->ctx == 0) {
        attr.majorVersion = 1;
        attr.minorVersion = 0;
        m_effectGLEnv->glesVersion = 2;
        m_effectGLEnv->ctx = emscripten_webgl_create_context(canvas, &attr);
    }
    printf("Created context with handle %u, webgl version: %u\n", (unsigned int)m_effectGLEnv->ctx, (unsigned int)attr.majorVersion);
    emscripten_webgl_make_context_current(m_effectGLEnv->ctx);
    if (m_effectGLEnv->ctx != 0) {
        m_effectGLEnv->initialized = true;
        m_effectGLEnv->glesVersion = attr.majorVersion;
        return true;
    }
    else {
        m_effectGLEnv->initialized = false;
        m_effectGLEnv->glesVersion = -1;
    }
    return false;
}
#endif

bool BEFEffectGLContext::initGLContext() {
    if (m_effectGLEnv->initialized) {
        LOGS("u already have glContext, u need releaseGLContext before initGLContext");
        return false;
    }
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    return initGLContext("#canvas");
#else
    EGLint eglMajVers, eglMinVers;

#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS
    if (m_effectGLEnv->hWnd != NULL) {
        HDC hdc = GetDC((HWND)(m_effectGLEnv->hWnd));
        m_effectGLEnv->eglDisplay = eglGetDisplay(hdc);
    }
    else
#endif
        m_effectGLEnv->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (m_effectGLEnv->eglDisplay == EGL_NO_DISPLAY)
    {
        //Unable to open connection to local windowing system
        LOGS("Unable to open connection to local windowing system");
        return false;
    }
#if defined (BEF_EFFECT_PLATFORM_WINDOWS)

    bool d3d11 = true;
    if(!eglInitialize(m_effectGLEnv->eglDisplay, &eglMajVers, &eglMinVers))
    {
        // Unable to initialize EGL. Handle and recover
        LOGS("Unable to initialize EGL through eglGetDisplay");
        d3d11 = false;
        m_bUsePbo = false;
    }

    if (!d3d11) {
        std::vector<EGLAttrib> displayAttributes;
        EGLPlatformParameters params;
        params.renderer = EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(params.renderer);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(params.majorVersion);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(params.minorVersion);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(params.deviceType);
        displayAttributes.push_back(EGL_NONE);


        LOGS("eglGetPlatformDisplay begin");
        m_effectGLEnv->eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
            EGL_DEFAULT_DISPLAY,
            &displayAttributes[0]);
        LOGS("eglGetPlatformDisplay end");

        LOGS("eglGetPlatformDisplay eglInitialize begin");
        if (!eglInitialize(m_effectGLEnv->eglDisplay, &eglMajVers, &eglMinVers))
        {
            // Unable to initialize EGL. Handle and recover
            LOGS("Unable to initialize EGL through eglGetPlatformDisplay");
            return false;
        }
        LOGS("eglGetPlatformDisplay eglInitialize end");
    }
#else
    if (!eglInitialize(m_effectGLEnv->eglDisplay, &eglMajVers, &eglMinVers))
    {
        // Unable to initialize EGL. Handle and recover
        LOGS("Unable to initialize EGL");
        return false;
    }
#endif
    LOGS("EGL init with version %d.%d", eglMajVers, eglMinVers);
#ifdef EGL_OPENGL_ES3_BIT
    if (createGLESContext(3)) {
        LOGS("Create ES3 Context Success");
    }
    else
#endif
        if (createGLESContext(2)) {
            LOGS("Create ES2 Context Success");
        }
        else {
            m_effectGLEnv->eglDisplay = EGL_NO_DISPLAY;
            LOGS("Create ESX Context Failed");
            return false;
        }

    if (!eglMakeCurrent(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface, m_effectGLEnv->eglSurface, m_effectGLEnv->eglContext))
    {
        LOGS("MakeCurrent failed");
        eglDestroyContext(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglContext);
        eglDestroySurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface);
        m_effectGLEnv->eglContext = EGL_NO_CONTEXT;
        m_effectGLEnv->eglSurface = EGL_NO_SURFACE;
        m_effectGLEnv->eglDisplay = EGL_NO_DISPLAY;
        return false;
    }
    if (!eglMakeCurrent(m_effectGLEnv->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
    {
        LOGS("MakeCurrent EGL_NO_CONTEXT failed");
        eglDestroyContext(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglContext);
        eglDestroySurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface);
        m_effectGLEnv->eglContext = EGL_NO_CONTEXT;
        m_effectGLEnv->eglSurface = EGL_NO_SURFACE;
        m_effectGLEnv->eglDisplay = EGL_NO_DISPLAY;
        return false;
    }
    m_effectGLEnv->initialized = true;
    LOGS("initialize success!");
    return true;
#endif
}

bool BEFEffectGLContext::releaseGLContext() {
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    if (m_effectGLEnv->ctx == 0) {
        LOGS("u need initGLContext before releaseGLContext");
        return false;
    }
    emscripten_webgl_destroy_context(m_effectGLEnv->ctx);
#else
    if (!m_effectGLEnv->initialized) {
        LOGS("u need initGLContext before releaseGLContext");
        return false;
    }
    eglDestroyContext(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglContext);
    eglDestroySurface(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface);
    m_effectGLEnv->eglContext = EGL_NO_CONTEXT;
    m_effectGLEnv->eglSurface = EGL_NO_SURFACE;
    m_effectGLEnv->eglDisplay = EGL_NO_DISPLAY;
    m_effectGLEnv->initialized = false;
#endif
    return true;
}

bool BEFEffectGLContext::makeCurrentContext() {
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    if (m_effectGLEnv->ctx == 0)
    {
        LOGS("MakeCurrent failed");
        return false;
    }
    emscripten_webgl_make_context_current(m_effectGLEnv->ctx);
#else
    if (!m_effectGLEnv->initialized || !eglMakeCurrent(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface, m_effectGLEnv->eglSurface, m_effectGLEnv->eglContext))
    {
        LOGS("MakeCurrent failed");
        return false;
    }
#endif
    return true;
}

bool BEFEffectGLContext::doneCurrentContext() {
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    //emscripten_webgl_make_context_current(0);
#else
    if (!m_effectGLEnv->initialized || !eglMakeCurrent(m_effectGLEnv->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
    {
        LOGS("MakeCurrent failed");
        return false;
    }
#endif
    return true;
}

bool BEFEffectGLContext::swapBuffers() {
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    if (m_effectGLEnv->ctx) {
        emscripten_webgl_commit_frame();
        return true;
    }
#else
    if (m_effectGLEnv->initialized) {
        eglSwapBuffers(m_effectGLEnv->eglDisplay, m_effectGLEnv->eglSurface);
        return true;
    }
#endif
    return false;
}

NAMESPACE_BEF_EFFECT_FRAMEWORK_END
