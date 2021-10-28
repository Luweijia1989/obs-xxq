#ifndef BE_RENDER_HELPER_H
#define BE_RENDER_HELPER_H


#include "bef_effect_ai_public_define.h"
#ifdef USE_ANGLE
#ifdef USE_DYNAMIC_ANGLE
#include "bef_effect_ai_load_library.h"
#include "egl_loader_autogen.h"
#include "gles_loader_autogen.h"
#include "system_utils.h"
#else
#include <GLES2/gl2.h>
#endif
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif
#include "be_render_define.h"
#include "be_program.h"

class BERenderHelper
{
public: 
    BERenderHelper();
    ~BERenderHelper();

    void init();
    void destroy();

    void setViewWidthAndHeight(int width, int height);
    void setResizeRatio(float ratio);

    void drawPoint(int x, int y, const be_rgba_color &color, float pointSize);
    void drawPoints(bef_ai_fpoint *points,int count, const be_rgba_color &color, float pointSize);

    void drawLine(be_render_helper_line *line, const be_rgba_color &color, float lineWidth);
    void drawLines(bef_ai_fpoint *lines, int count, const be_rgba_color &color, float lineWidth);
    void drawLinesStrip(bef_ai_fpoint *lines, int count, const be_rgba_color &color, float lineWidth);
    void drawRect(bef_ai_rect *rect, const be_rgba_color &color, float lineWidth);
    void drawNumberTexture(bef_ai_fpoint point, const be_rgba_color& color, unsigned char* image, int width, int height);

    void drawTexture(GLuint texture);
    void drawMask(unsigned char *mask, GLuint currentTexture, const be_rgba_color &color, int *size);
    void drawMask(unsigned char* mask, GLuint currentTexture, float* affine, const be_rgba_color& color, int* size);
    void drawMask(unsigned char* mask, GLuint currentTexture, GLuint texture, int* size);
    void drawFaceSegment(unsigned char* mask, const be_rgba_color& color, GLuint currentTexture, float* affine, int* size);
   
    void drawPortraitMask(unsigned char *mask, const be_rgba_color &color, GLuint backgroundTexture, int *size);

    void textureToImage(GLuint texture, unsigned char * buffer, int width, int height);
    int compileShader(const char* shader, GLenum shaderType);

private:
    void loadPointShader();
    void loadLineShader();
    void loadResizeShader();
    void loadMaskShader();
    void loadMaskFaceShader();
    //void loadMaskPortraitShader();
    void loadNumberShader();

    void checkGLError();

    float transformX(int x);
    float transformY(int y);

private:

    BEPointProgram* m_pointProgram;
    BELineProgram* m_lineProgram;
    BEColorMaskProgram* m_colorMaskProgram;
    BEColorMaskProgram* m_bgColorMaskProgram;
    BETextureMaskProgram* m_textureMaskProgram;
    BETextureMaskProgram* m_bgTextureMaskProgram;
    BEAffineColorMaskProgram* m_affineColorMaskProgram;
    BEResizeTextureProgram* m_resizeTextureProgram;
    BEDrawFaceMaskProgram* m_faceMaskProgram;
    BEDrawNumberTextureProgram* m_drawNumberProgram;

    int m_width;
    int m_height;
    int m_viewWidth;
    int m_viewHeight;

    float m_ratio;
};

#endif // !BE_RENDER_HELPER_H
