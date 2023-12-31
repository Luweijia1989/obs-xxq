// GENERATED FILE - DO NOT EDIT.
// Generated by generate_loader.py using data from egl.xml and egl_angle_ext.xml.
//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// egl_loader_autogen.h:
//   Simple EGL function loader.

#ifndef UTIL_EGL_LOADER_AUTOGEN_H_
#define UTIL_EGL_LOADER_AUTOGEN_H_

#include "util_export.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

ANGLE_UTIL_EXPORT extern PFNEGLCHOOSECONFIGPROC eglChooseConfig;
ANGLE_UTIL_EXPORT extern PFNEGLCOPYBUFFERSPROC eglCopyBuffers;
ANGLE_UTIL_EXPORT extern PFNEGLCREATECONTEXTPROC eglCreateContext;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPIXMAPSURFACEPROC eglCreatePixmapSurface;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEWINDOWSURFACEPROC eglCreateWindowSurface;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYCONTEXTPROC eglDestroyContext;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYSURFACEPROC eglDestroySurface;
ANGLE_UTIL_EXPORT extern PFNEGLGETCONFIGATTRIBPROC eglGetConfigAttrib;
ANGLE_UTIL_EXPORT extern PFNEGLGETCONFIGSPROC eglGetConfigs;
ANGLE_UTIL_EXPORT extern PFNEGLGETCURRENTDISPLAYPROC eglGetCurrentDisplay;
ANGLE_UTIL_EXPORT extern PFNEGLGETCURRENTSURFACEPROC eglGetCurrentSurface;
ANGLE_UTIL_EXPORT extern PFNEGLGETDISPLAYPROC eglGetDisplay;
ANGLE_UTIL_EXPORT extern PFNEGLGETERRORPROC eglGetError;
ANGLE_UTIL_EXPORT extern PFNEGLGETPROCADDRESSPROC eglGetProcAddress;
ANGLE_UTIL_EXPORT extern PFNEGLINITIALIZEPROC eglInitialize;
ANGLE_UTIL_EXPORT extern PFNEGLMAKECURRENTPROC eglMakeCurrent;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYCONTEXTPROC eglQueryContext;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSTRINGPROC eglQueryString;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSURFACEPROC eglQuerySurface;
ANGLE_UTIL_EXPORT extern PFNEGLSWAPBUFFERSPROC eglSwapBuffers;
ANGLE_UTIL_EXPORT extern PFNEGLTERMINATEPROC eglTerminate;
ANGLE_UTIL_EXPORT extern PFNEGLWAITGLPROC eglWaitGL;
ANGLE_UTIL_EXPORT extern PFNEGLWAITNATIVEPROC eglWaitNative;
ANGLE_UTIL_EXPORT extern PFNEGLBINDTEXIMAGEPROC eglBindTexImage;
ANGLE_UTIL_EXPORT extern PFNEGLRELEASETEXIMAGEPROC eglReleaseTexImage;
ANGLE_UTIL_EXPORT extern PFNEGLSURFACEATTRIBPROC eglSurfaceAttrib;
ANGLE_UTIL_EXPORT extern PFNEGLSWAPINTERVALPROC eglSwapInterval;
ANGLE_UTIL_EXPORT extern PFNEGLBINDAPIPROC eglBindAPI;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYAPIPROC eglQueryAPI;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC eglCreatePbufferFromClientBuffer;
ANGLE_UTIL_EXPORT extern PFNEGLRELEASETHREADPROC eglReleaseThread;
ANGLE_UTIL_EXPORT extern PFNEGLWAITCLIENTPROC eglWaitClient;
ANGLE_UTIL_EXPORT extern PFNEGLGETCURRENTCONTEXTPROC eglGetCurrentContext;
ANGLE_UTIL_EXPORT extern PFNEGLCREATESYNCPROC eglCreateSync;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYSYNCPROC eglDestroySync;
ANGLE_UTIL_EXPORT extern PFNEGLCLIENTWAITSYNCPROC eglClientWaitSync;
ANGLE_UTIL_EXPORT extern PFNEGLGETSYNCATTRIBPROC eglGetSyncAttrib;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEIMAGEPROC eglCreateImage;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYIMAGEPROC eglDestroyImage;
ANGLE_UTIL_EXPORT extern PFNEGLGETPLATFORMDISPLAYPROC eglGetPlatformDisplay;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPLATFORMWINDOWSURFACEPROC eglCreatePlatformWindowSurface;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC eglCreatePlatformPixmapSurface;
ANGLE_UTIL_EXPORT extern PFNEGLWAITSYNCPROC eglWaitSync;
ANGLE_UTIL_EXPORT extern PFNEGLSETBLOBCACHEFUNCSANDROIDPROC eglSetBlobCacheFuncsANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETCOMPOSITORTIMINGANDROIDPROC eglGetCompositorTimingANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETCOMPOSITORTIMINGSUPPORTEDANDROIDPROC
    eglGetCompositorTimingSupportedANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETFRAMETIMESTAMPSUPPORTEDANDROIDPROC
    eglGetFrameTimestampSupportedANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETFRAMETIMESTAMPSANDROIDPROC eglGetFrameTimestampsANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETNEXTFRAMEIDANDROIDPROC eglGetNextFrameIdANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLPRESENTATIONTIMEANDROIDPROC eglPresentationTimeANDROID;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEDEVICEANGLEPROC eglCreateDeviceANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLRELEASEDEVICEANGLEPROC eglReleaseDeviceANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYDISPLAYATTRIBANGLEPROC eglQueryDisplayAttribANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSTRINGIANGLEPROC eglQueryStringiANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLPROGRAMCACHEGETATTRIBANGLEPROC eglProgramCacheGetAttribANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLPROGRAMCACHEPOPULATEANGLEPROC eglProgramCachePopulateANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLPROGRAMCACHEQUERYANGLEPROC eglProgramCacheQueryANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLPROGRAMCACHERESIZEANGLEPROC eglProgramCacheResizeANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSURFACEPOINTERANGLEPROC eglQuerySurfacePointerANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLCREATESTREAMPRODUCERD3DTEXTUREANGLEPROC
    eglCreateStreamProducerD3DTextureANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMPOSTD3DTEXTUREANGLEPROC eglStreamPostD3DTextureANGLE;
//ANGLE_UTIL_EXPORT extern PFNEGLSWAPBUFFERSWITHFRAMETOKENANGLEPROC eglSwapBuffersWithFrameTokenANGLE;
ANGLE_UTIL_EXPORT extern PFNEGLGETSYNCVALUESCHROMIUMPROC eglGetSyncValuesCHROMIUM;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC eglCreatePlatformPixmapSurfaceEXT;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT;
ANGLE_UTIL_EXPORT extern PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
ANGLE_UTIL_EXPORT extern PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
ANGLE_UTIL_EXPORT extern PFNEGLLABELOBJECTKHRPROC eglLabelObjectKHR;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYDEBUGKHRPROC eglQueryDebugKHR;
ANGLE_UTIL_EXPORT extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
ANGLE_UTIL_EXPORT extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
ANGLE_UTIL_EXPORT extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;
ANGLE_UTIL_EXPORT extern PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
ANGLE_UTIL_EXPORT extern PFNEGLCREATESTREAMKHRPROC eglCreateStreamKHR;
ANGLE_UTIL_EXPORT extern PFNEGLDESTROYSTREAMKHRPROC eglDestroyStreamKHR;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSTREAMKHRPROC eglQueryStreamKHR;
ANGLE_UTIL_EXPORT extern PFNEGLQUERYSTREAMU64KHRPROC eglQueryStreamu64KHR;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMATTRIBKHRPROC eglStreamAttribKHR;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMCONSUMERACQUIREKHRPROC eglStreamConsumerAcquireKHR;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC
    eglStreamConsumerGLTextureExternalKHR;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMCONSUMERRELEASEKHRPROC eglStreamConsumerReleaseKHR;
ANGLE_UTIL_EXPORT extern PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC eglSwapBuffersWithDamageKHR;
ANGLE_UTIL_EXPORT extern PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
ANGLE_UTIL_EXPORT extern PFNEGLPOSTSUBBUFFERNVPROC eglPostSubBufferNV;
ANGLE_UTIL_EXPORT extern PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC
    eglStreamConsumerGLTextureExternalAttribsNV;

namespace angle
{
using GenericProc = void (*)();
using LoadProc    = GenericProc(KHRONOS_APIENTRY *)(const char *);
void LoadEGL(LoadProc loadProc);
}  // namespace angle

#endif  // UTIL_EGL_LOADER_AUTOGEN_H_
