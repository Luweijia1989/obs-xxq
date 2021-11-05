#ifndef BE_PROGRAM_H
#define BE_PROGRAM_H

#include "be_render_define.h"
#include "bef_effect_ai_public_define.h"
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

class BEGLProgram {
public:
	BEGLProgram();
	virtual ~BEGLProgram();

	virtual void initialize(const char* vertext, const char* fragment);
	GLuint compileShader(const char* shader, GLenum shaderType);
protected:
	GLuint m_program;
	GLuint m_position;
	GLuint m_color;
};

class BEPointProgram : public BEGLProgram {
public:
	void initialize(const char* vertext, const char* fragment);
	void drawPoint(float x, float y, be_rgba_color color, float pointSize);
	void drawPoints(bef_ai_fpoint* points, int count, const be_rgba_color& color, float pointSize);
private:
	GLuint m_size;
};

class BELineProgram : public BEGLProgram {
public:
	void initViewSize(int width, int height);
	void drawLine(be_render_helper_line* line, const be_rgba_color& color, float lineWidth);
	void drawLines(bef_ai_fpoint* lines, int count, const be_rgba_color& color, float lineWidth);
	void drawLineStrip(bef_ai_fpoint* lines, int count, const be_rgba_color& color, float lineWidth);
	void drawRect(bef_ai_rectf* rect, const be_rgba_color& color, float lineWidth);
	void draw(GLfloat* positions, int count, const be_rgba_color& color, float lineWidth, GLenum mode);
private:
	int m_viewWidth;
	int m_viewHeight;
};

class BEMaskProgram : public BEGLProgram {
public:
	BEMaskProgram();
	~BEMaskProgram();

	virtual void initialize(const char* vertext, const char* fragment);
	virtual void bindData() {}
	void draw(unsigned char* mask, int* size);
private:
	GLuint m_maskTexture;
	GLuint m_maskCoordinateLocation;
	GLuint m_cacheTexture;
};

class BEColorMaskProgram : public BEMaskProgram {
public:
	void initialize(const char* vertext, const char* fragment);
	void drawMask(unsigned char* mask, const be_rgba_color& color, int* size);
	virtual void bindData();
protected:
	GLuint m_maskColor;
	be_rgba_color m_color;
};

class BETextureMaskProgram : public BEMaskProgram {
public:
	void initialize(const char* vertext, const char* fragment);
	void drawMask(unsigned char* mask, GLuint texture, int* size);
	virtual void bindData();

private:
	GLuint m_maskTexture;
	GLuint m_texture;
};

class BEAffineColorMaskProgram : BEColorMaskProgram {
public:
	void initialize(const char* vertext, const char* fragment);
	void drawMask(unsigned char* mask, const be_rgba_color& color, int* size, float *affine, int width, int height);
	virtual void bindData();

private:
	GLuint m_maskWidth;
	GLuint m_maskHeight;
	GLuint m_viewportWidth;
	GLuint m_viewportHeight;
	GLuint m_affine;

	float* m_affineData;
	be_rgba_color m_color;
	int m_width;
	int m_height;
	int m_size[2];
};

class BEResizeTextureProgram : public BEGLProgram {
public:
	BEResizeTextureProgram();
	~BEResizeTextureProgram();

	void initialize(const char* vertext, const char* fragment);
	void textureToImage(GLuint texture, unsigned char* buffer, int width, int height, GLenum format);
	void drawTexture(GLuint texture);

protected:
	GLuint m_textureCoordinate;
	GLuint m_inputTexture;
	GLuint m_frameBuffer;
	GLuint m_resizedTexture;
};

class BEDrawNumberTextureProgram : public BEGLProgram {
public:
	BEDrawNumberTextureProgram();
	~BEDrawNumberTextureProgram();

	void initialize(const char* vertext, const char* fragment);
	void drawNumber(bef_ai_rectf& rect, unsigned char* image, int width, int height);

private:
	GLuint m_textureCoordLocation;
	GLuint m_inputTexture;
	GLuint m_cacheTexture;
	
};

class BEDrawFaceMaskProgram : public BEGLProgram {
public:
	BEDrawFaceMaskProgram();
	~BEDrawFaceMaskProgram();

	void initialize(const char* vertext, const char* fragment);
	void drawSegment(unsigned char* mask, float *affine, const be_rgba_color& color, GLuint texture, int *size, int width, int height);
private:
	GLuint m_faceMaskTextureCoordinate;
	GLuint m_faceMaskSeg;
	GLuint m_faceMaskWrapMat;
	GLuint m_faceMaskColor;
	GLuint m_faceMaskTexture;
};

#endif
