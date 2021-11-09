#include "be_render_helper.h"



const char* LINE_VERTEX =
"attribute vec4 position;\n"
"void main() {\n"
"    gl_Position = position;\n"
"}\0";

const char* LINE_FRAGMENT =
"precision mediump float;\n"
"uniform vec4 color;\n"
"void main() {\n"
"    gl_FragColor = color;\n"
"}\0";

const char* POINT_VERTEX =
"attribute vec4 position;\n"
"uniform float uPointSize;\n"
"void main() {\n"
"    gl_Position = position;\n"
"    gl_PointSize = uPointSize;\n"
"}\0";

const char* POINT_FRAGMENT = 
"precision mediump float;\n"
"uniform vec4 color;\n"
"void main() {\n"
"    gl_FragColor = color;\n"
"}\0";

const char* CAMREA_RESIZE_VERTEX =
"attribute vec4 position;\n"
"attribute vec4 inputTextureCoordinate;\n"
"varying vec2 textureCoordinate;\n"
"void main() {\n"
"    textureCoordinate = inputTextureCoordinate.xy;\n"
"    gl_Position = position;\n"
"}\0";

const char* CAMREA_RESIZE_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputImageTexture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(inputImageTexture, textureCoordinate);\n"
"}\0";

const char* MASK_VERTEX =
"attribute vec4 position;\n"
"attribute vec4 inputTextureCoordinate;\n"
"varying vec2 textureCoordinate;\n"
"void main() {\n"
"    textureCoordinate = vec2(inputTextureCoordinate.x, 1.0 - inputTextureCoordinate.y);\n"
"    gl_Position = position;\n"
"}\0";

const char* MASK_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform vec4 maskColor;\n"
"void main() {\n"
"    float maska = texture2D(inputMaskTexture, textureCoordinate).a;\n"
"    gl_FragColor = vec4(maskColor.rgb, maska * maskColor.a);\n"
"}\0";

const char* MASK_BG_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform vec4 maskColor;\n"
"void main() {\n"
"    float maska = texture2D(inputMaskTexture, textureCoordinate).a;\n"
"    gl_FragColor = vec4(maskColor.rgb, 1.0 - maska);\n"
"}\0";

const char* MASK_WITH_TEXTURE_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform sampler2D forgroundTexture;\n"
"void main() {\n"
"    float maska = texture2D(inputMaskTexture, textureCoordinate).a;\n"
"    vec4 maskColor = texture2D(forgroundTexture, textureCoordinate);\n"
"    gl_FragColor = vec4(maskColor.rgb, maska * maskColor.a);\n"
"}\0";

const char* MASK_WITH_BG_TEXTURE_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform sampler2D forgroundTexture;\n"
"void main() {\n"
"    float maska = texture2D(inputMaskTexture, textureCoordinate).a;\n"
"    vec4 maskColor = texture2D(forgroundTexture, textureCoordinate);\n"
"    gl_FragColor = vec4(maskColor.rgb, (1.0 - maska) * maskColor.a);\n"
"}\0";

const char* MASK_AFFINE_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform mat3 affine;\n"
"uniform float viewport_width;\n"
"uniform float viewport_height;\n"
"uniform float mask_width;\n"
"uniform float mask_height;\n"
"uniform vec4 maskColor;\n"

"void main() {\n"
"   vec2 image_coord = vec2(textureCoordinate.x * viewport_width, textureCoordinate.y * viewport_height);\n"
"   vec3 affine_coord = affine * vec3(image_coord, 1.0);\n"
"   vec2 tex_coord = vec2(affine_coord.x / mask_width, affine_coord.y / mask_height);\n"
"   if (tex_coord.x > 1.0 || tex_coord.x < 0.0 || tex_coord.y > 1.0 || tex_coord.y < 0.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0); return; }\n"
"   float maska = texture2D(inputMaskTexture, tex_coord).a;\n"
"   gl_FragColor = vec4(maskColor.rgb, maska * maskColor.a);\n"
"}\0";

const char* MASK_FACE_VERTEX =
"attribute vec4 position;\n"
"attribute vec4 inputTextureCoordinate;\n"
"\n"
"varying vec2 textureCoordinate;\n"
"varying vec2 maskCoordinate;\n"
"uniform mat4 warpMat;\n"
"\n"
"void main()\n"
"{\n"
"	textureCoordinate = vec2(inputTextureCoordinate.x, 1.0 - inputTextureCoordinate.y);\n"
"	gl_Position = position;\n"
"	maskCoordinate = (warpMat * vec4(textureCoordinate, 0.0, 1.0)).xy;\n"
"}";

const char* MASK_FACE_FRAGMENT =
"precision mediump float;\n"
"varying highp vec2 textureCoordinate;\n"
"varying highp vec2 maskCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform vec3 maskColor;\n"
"void main()\n"
"{\n"
"   vec4 color = vec4(0.0, 0.0, 0.0, 0.0);\n"
"\n"
"   vec2 maskCoordinate1 = vec2(maskCoordinate.x, maskCoordinate.y);\n"
//                            "   if(clamp(maskCoordinate1, 0.0, 1.0) != maskCoordinate1)\n"
"   if(maskCoordinate1.x > 1.0 || maskCoordinate1.x < 0.0 || maskCoordinate1.y > 1.0 || maskCoordinate1.y < 0.0 )\n"
"   {\n"
"       gl_FragColor = color;\n"
"   }\n"
"   else\n"
"   {\n"
"       float mask = texture2D(inputMaskTexture, maskCoordinate1).a;\n"
"       gl_FragColor = vec4(maskColor, mask*0.45);\n"
"   }\n"
"}";

const char* MASK_PORTRAIT_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputMaskTexture;\n"
"uniform vec4 maskColor;\n"
"void main() {\n"
"    float maska = texture2D(inputMaskTexture, textureCoordinate).a;\n"
"    gl_FragColor = vec4(maskColor.rgb, 1.0 - maska);\n"
"}\0";

const char* TEXTURE_VERTEX =
"attribute vec4 position;\n"
"attribute vec4 inputTextureCoordinate;\n"
"varying vec2 textureCoordinate;\n"
"void main() {\n"
"    textureCoordinate = inputTextureCoordinate.xy;\n"
"    gl_Position = position;\n"
"}\0";

const char* TEXTURE_FRAGMENT =
"precision mediump float;\n"
"varying vec2 textureCoordinate;\n"
"uniform sampler2D inputImageTexture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(inputImageTexture, textureCoordinate);\n"
"}\0";


BERenderHelper::BERenderHelper()
{
    m_pointProgram = nullptr;
    m_lineProgram = nullptr;
    m_colorMaskProgram = nullptr;
    m_bgColorMaskProgram = nullptr;
    m_textureMaskProgram = nullptr;
    m_bgTextureMaskProgram = nullptr;
    m_affineColorMaskProgram = nullptr;
    m_resizeTextureProgram = nullptr;
    m_faceMaskProgram = nullptr;
    m_drawNumberProgram = nullptr;
}


BERenderHelper::~BERenderHelper()
{
    destroy();
}

void BERenderHelper::init() {
    loadPointShader();
    loadLineShader();
    loadResizeShader();
    loadMaskShader();
  //  loadMaskPortraitShader();
    loadNumberShader();

    m_viewWidth = 1280;
    m_viewHeight = 720;

    m_ratio = 1.0;
}

void BERenderHelper::destroy() {

    if (m_pointProgram) {
        delete m_pointProgram;
        m_pointProgram = nullptr;
    }

    if (m_lineProgram) {
        delete m_lineProgram;
        m_lineProgram = nullptr;
    }

    if (m_colorMaskProgram) {
        delete m_colorMaskProgram;
        m_colorMaskProgram = nullptr;
    }

    if (m_bgColorMaskProgram) {
        delete m_bgColorMaskProgram;
        m_bgColorMaskProgram = nullptr;
    }

    if (m_textureMaskProgram) {
        delete m_textureMaskProgram;
        m_textureMaskProgram = nullptr;
    }

    if (m_bgTextureMaskProgram) {
        delete m_bgTextureMaskProgram;
        m_bgTextureMaskProgram = nullptr;
    }

    if (m_affineColorMaskProgram) {
        delete m_affineColorMaskProgram;
        m_affineColorMaskProgram = nullptr;
    }

    if (m_resizeTextureProgram) {
        delete m_resizeTextureProgram;
        m_resizeTextureProgram = nullptr;
    }

    if (m_faceMaskProgram) {
        delete m_faceMaskProgram;
        m_faceMaskProgram = nullptr;
    }

    if (m_drawNumberProgram) {
        delete m_drawNumberProgram;
        m_drawNumberProgram = nullptr;
    }
}

void BERenderHelper::setViewWidthAndHeight(int width, int height){
    m_viewWidth = width;
    m_viewHeight = height;
}

void BERenderHelper::setResizeRatio(float ratio){
    m_ratio = ratio;
}

void BERenderHelper::drawPoint(int x, int y, const be_rgba_color &color, float pointSize) {
    float transX = transformX(x);
    float transY = transformY(y);

    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_pointProgram->drawPoint(transX, transY, color, pointSize);
    checkGLError();
}

void BERenderHelper::drawPoints(bef_ai_fpoint *points, int count, const be_rgba_color &color, float pointSize) {
    bef_ai_fpoint* be_points = new bef_ai_fpoint[count];
    for (int i = 0; i < count; i++) {
        be_points[i].x = transformX(points[i].x);
        be_points[i].y = transformY(points[i].y);
    }

    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_pointProgram->drawPoints(be_points, count, color, pointSize);
    checkGLError();
    delete[] be_points;
}

void BERenderHelper::drawLine(be_render_helper_line *line, const be_rgba_color &color, float lineWidth) {
    be_render_helper_line be_line;
    be_line.x1 = transformX(line->x1);
    be_line.y1 = transformY(line->y1);
    be_line.x2 = transformX(line->x2);
    be_line.y2 = transformY(line->y2);

    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_lineProgram->initViewSize(m_viewWidth, m_viewHeight);
    m_lineProgram->drawLine(&be_line, color, lineWidth);
    checkGLError();
}

void BERenderHelper::drawLines(bef_ai_fpoint *lines, int count, const be_rgba_color &color, float lineWidth) {
    if (count <= 0) return;

    bef_ai_fpoint* fLines = new bef_ai_fpoint[count];
    for (int i = 0; i < count; i++) {
        fLines[i].x = transformX(lines[i].x);
        fLines[i].y = transformY(lines[i].y);
    }
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_lineProgram->initViewSize(m_viewWidth, m_viewHeight);
    m_lineProgram->drawLines(fLines, count, color, lineWidth);
    checkGLError();
    delete[] fLines;
}

void BERenderHelper::drawLinesStrip(bef_ai_fpoint *lines, int count, const be_rgba_color &color, float lineWidth) {
    bef_ai_fpoint *fLines = new bef_ai_fpoint[count];
    for (int i = 0; i < count; i++) {
        fLines[i].x = transformX(lines[i].x);
        fLines[i].y = transformY(lines[i].y);
    }
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_lineProgram->initViewSize(m_viewWidth, m_viewHeight);
    m_lineProgram->drawLineStrip(fLines, count, color, lineWidth);
    checkGLError();
    delete[] fLines;
}


void BERenderHelper::drawRect(bef_ai_rect *rect, const be_rgba_color &color, float lineWidth) {
    bef_ai_rectf rc;
    rc.left = transformX(rect->left);
    rc.top = transformY(rect->top);
    rc.right = transformX(rect->right);
    rc.bottom = transformY(rect->bottom);

    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_lineProgram->initViewSize(m_viewWidth, m_viewHeight);
    m_lineProgram->drawRect(&rc, color, lineWidth);
    checkGLError();
}

void BERenderHelper::drawNumberTexture(bef_ai_fpoint point, const be_rgba_color& color, unsigned char* image, int width, int height) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);

    bef_ai_rectf rectf;
    rectf.left = transformX(point.x);
    rectf.top = transformY(point.y);
    rectf.right = transformX(point.x + 30);
    rectf.bottom = transformY(point.y + 30);
    m_drawNumberProgram->drawNumber(rectf, image, width, height);
    checkGLError();
}

void BERenderHelper::drawTexture(GLuint texture) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_resizeTextureProgram->drawTexture(texture);
    checkGLError();
}

void BERenderHelper::drawMask(unsigned char *mask, GLuint currentTexture, const be_rgba_color &color, int *size) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_colorMaskProgram->drawMask(mask, color, size);
    checkGLError();
}

void BERenderHelper::drawMask(unsigned char* mask, GLuint currentTexture, GLuint texture, int* size) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_textureMaskProgram->drawMask(mask, texture, size);
    checkGLError();
}

void BERenderHelper::drawMask(unsigned char* mask, GLuint currentTexture, float* affine, const be_rgba_color& color, int* size) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_affineColorMaskProgram->drawMask(mask, color, size, affine, m_viewWidth, m_viewHeight);
    checkGLError();
}

void BERenderHelper::drawFaceSegment(unsigned char* mask, const be_rgba_color& color, GLuint currentTexture,
                                    float * affine, int* size) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_faceMaskProgram->drawSegment(mask, affine, color, currentTexture, size, m_viewWidth, m_viewHeight);
    checkGLError();
}


void BERenderHelper::drawPortraitMask(unsigned char *mask, const be_rgba_color &color, GLuint backgroundTexture, int *size) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    //m_bgTextureMaskProgram->drawMask(mask, backgroundTexture, size);
    m_bgColorMaskProgram->drawMask(mask, color, size);
}

void BERenderHelper::textureToImage(GLuint texture, unsigned char * buffer, int width, int height) {
    glViewport(0, 0, m_viewWidth, m_viewHeight);
    m_resizeTextureProgram->textureToImage(texture, buffer, width, height, GL_RGBA);
    checkGLError();
}

int BERenderHelper::compileShader(const char* shader, GLenum shaderType) {
    GLuint shaderHandle = glCreateShader(shaderType);
    glShaderSource(shaderHandle, 1, &shader, NULL);
    glCompileShader(shaderHandle);
    GLint success;
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE)
    {
        printf("BErenderHelper compiler shader error: %s\n", shader);
        GLchar infoLog[1024];
        glGetShaderInfoLog(shaderHandle, 1024, NULL, infoLog);
        printf("ERROR::SHADER_COMPILATION_ERROR of type: %s\n", infoLog);
        return 0;
    }
    return shaderHandle;
}

void BERenderHelper::loadPointShader() {
    m_pointProgram = new BEPointProgram();
    m_pointProgram->initialize(POINT_VERTEX, POINT_FRAGMENT);
    checkGLError();
}

void BERenderHelper::loadLineShader() {
    m_lineProgram = new BELineProgram();
    m_lineProgram->initialize(LINE_VERTEX, LINE_FRAGMENT);
    checkGLError();
}


void BERenderHelper::loadResizeShader() {
    m_resizeTextureProgram = new BEResizeTextureProgram();
    m_resizeTextureProgram->initialize(CAMREA_RESIZE_VERTEX, CAMREA_RESIZE_FRAGMENT);
    checkGLError();
}

void BERenderHelper::loadMaskShader() {
    m_colorMaskProgram = new BEColorMaskProgram();
    m_colorMaskProgram->initialize(MASK_VERTEX, MASK_FRAGMENT);
    m_bgColorMaskProgram = new BEColorMaskProgram();
    m_bgColorMaskProgram->initialize(MASK_VERTEX, MASK_BG_FRAGMENT);
    m_textureMaskProgram = new BETextureMaskProgram();
    m_textureMaskProgram->initialize(MASK_VERTEX, MASK_WITH_TEXTURE_FRAGMENT);
    m_bgTextureMaskProgram = new BETextureMaskProgram();
    m_bgTextureMaskProgram->initialize(MASK_VERTEX, MASK_WITH_BG_TEXTURE_FRAGMENT);
    m_affineColorMaskProgram = new BEAffineColorMaskProgram();
    m_affineColorMaskProgram->initialize(MASK_VERTEX, MASK_AFFINE_FRAGMENT);

    m_faceMaskProgram = new BEDrawFaceMaskProgram();
    m_faceMaskProgram->initialize(MASK_FACE_VERTEX, MASK_FACE_FRAGMENT);
    checkGLError();
}

void BERenderHelper::loadNumberShader() {
    m_drawNumberProgram = new BEDrawNumberTextureProgram();
    m_drawNumberProgram->initialize(TEXTURE_VERTEX, TEXTURE_FRAGMENT);
    checkGLError();
}

void BERenderHelper::checkGLError() {
    //return;
    int error = glGetError();
    if (error != 0)
    {
        printf("BERenderHelper::checkGLError %d\n", error);
        //assert(error == GL_NO_ERROR);
    }
}

float BERenderHelper::transformX(int x) {
    return 2.0f * x / m_ratio / m_viewWidth - 1.0f;
}

float BERenderHelper::transformY(int y) {
    return 2.0f * y / m_ratio / m_viewHeight - 1.0f;
}

