#ifndef BE_RENDER_H
#define BE_RENDER_H


#include "be_render_helper.h"
#include "bef_effect_ai_face_detect.h"
#include "bef_effect_ai_face_attribute.h"
#include "bef_effect_ai_skeleton.h"
#include "bef_effect_ai_hand.h"
#include "bef_effect_ai_gaze_estimation.h"
#include "bef_effect_ai_pet_face.h"

#ifdef USE_ANGLE
#ifdef USE_DYNAMIC_ANGLE
#include "egl_loader_autogen.h"
#include "gles_loader_autogen.h"
#else
#include <GLES2/gl2.h>
#endif
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include <QImage>

class BERender
{
public:
    BERender();
    ~BERender();
    void initRender();
    void releaseRender();

    GLuint image2Texture(unsigned char *buffer, int width, int height);
    void texture2Image(GLuint texture, unsigned char* buffer, int width, int height);

    void setRenderHelperWidthHeight(int width, int height);
    void setRenderHelperResizeRatio(float ratio);
    void setRenderTargetTexture(GLuint texture);

    void drawFace(bef_ai_face_info *faceInfo, bool withExtra);
    void drawFaceRect(bef_ai_face_info * faceInfo);
    void drawFaceMask(bef_ai_mouth_mask_info* mouthMask, bef_ai_teeth_mask_info* teethMask, bef_ai_face_mask_info* faceMask);
    void drawSkeleton(bef_ai_skeleton_info *skeletonInfo, int validCount);
    void drawHands(bef_ai_hand_info *handInfo);
    void drawHairParse(unsigned char *mask, int *maskSize);
    void drawPortrait(unsigned char *mask, int *maskSize);
    void drawHeadSeg(unsigned char* mask, float* affine, int* maskSize);
    void drawGaze(bef_ai_gaze_estimation_info *gazeInfo);
    void drawPetFace(bef_ai_pet_face_result* petFaceInfo, std::vector<QImage>& numberImg);
private:
    void checkGLError();
private:
    BERenderHelper *m_renderHelper;
    GLuint m_frameBuffer;
    GLuint m_textureInput;
    GLuint m_textureOutput;
    GLuint m_currentTexture;

    float m_drawRatio;
    unsigned char *m_pixelBuffer = nullptr;
};

#endif // !BE_RENDER_H
