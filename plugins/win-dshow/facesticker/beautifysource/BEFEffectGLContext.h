//
// Created by brillian.ni on 2019/12/25.
//
#include "BEFPlatformDefine.h"

#ifndef BEF_EFFECTGLCONTEXT_H
#define BEF_EFFECTGLCONTEXT_H

NAMESPACE_BEF_EFFECT_FRAMEWORK_BEGIN

class BEFEffectGLContext {
public:
    BEFEffectGLContext();

    ~BEFEffectGLContext();

    bool initGLContext();

#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WASM
    bool initGLContext(const char* canvas);
#elif BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS
    typedef void* BEFWindow;
    bool initGLContext(BEFWindow hWnd);
#if USE_D3D
    bool initGLContextSharedWithD3D(void* sharedHandle = nullptr, int width = 1, int height = 1);
#endif
#endif

    bool releaseGLContext();

    bool makeCurrentContext();

    bool doneCurrentContext();

    bool swapBuffers();

    int getGLESVersion();

    bool usePbo() { return m_bUsePbo; }
private:
#if BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_WINDOWS || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_LINUX || BEF_EFFECT_PLATFORM == BEF_EFFECT_PLATFORM_ANDROID
    bool createGLESContext(int major);
#endif
    struct EffectGLEnv;
    EffectGLEnv* m_effectGLEnv = nullptr;
    bool m_bUsePbo;
};

NAMESPACE_BEF_EFFECT_FRAMEWORK_END

#endif //BEF_EFFECTGLCONTEXT_H
