#include "be_program.h"
#include <stdio.h>
#include <malloc.h>
#include <math.h>

static float TEXTURE_FLIPPED[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, };
static float TEXTURE_RORATION_0[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, };
static float TEXTURE_ROTATED_90[] = { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, };
static float TEXTURE_ROTATED_180[] = { 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, };
static float TEXTURE_ROTATED_270[] = { 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, };
static float CUBE[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, };

// 避免cos(1.5708)得出无穷大值
double cosX(double _x) {
	float cosData = cos(_x);
	if (fabsf(cosData) <= 0.00001f) {
		cosData = 0.0f;
	}
	return cosData;
}

BEGLProgram::BEGLProgram() {

}

BEGLProgram::~BEGLProgram() {
	glDeleteProgram(m_program);
}


void BEGLProgram::initialize(const char* vertext, const char* fragment) {
	GLuint vertexShader = compileShader(vertext, GL_VERTEX_SHADER);
	GLuint fragmentShader = compileShader(fragment, GL_FRAGMENT_SHADER);

	m_program = glCreateProgram();
	glAttachShader(m_program, vertexShader);
	glAttachShader(m_program, fragmentShader);
	glLinkProgram(m_program);

	GLint linkSuccess;
	glGetProgramiv(m_program, GL_LINK_STATUS, &linkSuccess);
	if (linkSuccess == GL_FALSE) {
		printf("BEGLProgram link shader error");
	}

	if (vertexShader != -1) {
		glDeleteShader(vertexShader);
	}

	if (fragmentShader != -1) {
		glDeleteShader(fragmentShader);
	}

	glUseProgram(m_program);
	m_position = glGetAttribLocation(m_program, "position");
	m_color = glGetUniformLocation(m_program, "color");
}

GLuint BEGLProgram::compileShader(const char* shader, GLenum shaderType) {
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

void BEPointProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);
	m_size = glGetUniformLocation(m_program, "uPointSize");
}

void BEPointProgram::drawPoint(float x, float y, be_rgba_color color, float pointSize) {
	GLfloat positions[] = {
	   x, y
	};

	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, positions);
	glEnableVertexAttribArray(m_position);

	glUniform4f(m_color, color.red, color.green, color.blue, color.alpha);
	glUniform1f(m_size, pointSize);
	glDrawArrays(GL_POINTS, 0, 1);
	glDisableVertexAttribArray(m_position);
}

void BEPointProgram::drawPoints(bef_ai_fpoint* points, int count, const be_rgba_color& color, float pointSize) {
	GLfloat *positions = (GLfloat*)malloc(count * 2 * sizeof(GLfloat));
	for (int i = 0; i < count; i++) {
		positions[i * 2] = points[i].x;
		positions[i * 2 + 1] = points[i].y;
	}

	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 2 * sizeof(GLfloat), positions);
	glEnableVertexAttribArray(m_position);

	glUniform4f(m_color, color.red, color.green, color.blue, color.alpha);
	glUniform1f(m_size, pointSize);
	glDrawArrays(GL_POINTS, 0, count);
	glDisableVertexAttribArray(m_position);
	free(positions);
}

void BELineProgram::initViewSize(int width, int height) {
	m_viewWidth = width;
	m_viewHeight = height;
}

void BELineProgram::drawLine(be_render_helper_line* line, const be_rgba_color& color, float lineWidth) {
	GLfloat positions[] = {
		line->x1, line->y1, line->x2, line->y2
	};

	draw(positions, 2, color, lineWidth, GL_LINES);
}

void BELineProgram::drawLines(bef_ai_fpoint* lines, int count, const be_rgba_color& color, float lineWidth) {
	if (count <= 0) return;

	GLfloat* positions = (GLfloat*)calloc(count, sizeof(GLfloat) * 2);
	for (int i = 0; i < count; i++) {
		positions[2 * i] = lines[i].x;
		positions[2 * i + 1] = lines[i].y;
	}

	draw(positions, count, color, lineWidth, GL_LINES);
}

void BELineProgram::drawLineStrip(bef_ai_fpoint* lines, int count, const be_rgba_color& color, float lineWidth) {
	if (count <= 0) return;

	GLfloat* positions = (GLfloat*)calloc(count, sizeof(GLfloat) * 2);
	for (int i = 0; i < count; i++) {
		positions[2 * i] = lines[i].x;
		positions[2 * i + 1] = lines[i].y;
	}

	draw(positions, count, color, lineWidth, GL_LINE_STRIP);
	free(positions);
}

void BELineProgram::drawRect(bef_ai_rectf* rect, const be_rgba_color& color, float lineWidth) {
	GLfloat positions[] = {
		rect->left, rect->top,
		rect->left, rect->bottom,
		rect->right, rect->bottom,
		rect->right, rect->top,
		rect->left, rect->top
	};

	draw(positions, 5, color, lineWidth, GL_LINE_STRIP);
}

void BELineProgram::draw(GLfloat* positions, int count, const be_rgba_color& color, float lineWidth, GLenum mode) {
	glUseProgram(m_program);
	/*glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 2 * sizeof(GLfloat), positions);
	glEnableVertexAttribArray(m_position);

	glUniform4f(m_color, color.red, color.green, color.blue, color.alpha);
	glLineWidth(lineWidth);
	glDrawArrays(mode, 0, count);*/

   int nums = (count + 1) / 2;
   for (int i = 0; i <= nums; ) {
       float xPos1 = positions[i * 2];
       float yPos1 = positions[i * 2 + 1];
       float xPos2 = positions[i * 2 + 2];
       float yPos2 = positions[i * 2 + 3];
       
       float t1 = (lineWidth + 1.0f) / (float)m_viewWidth;
       float t2 = (lineWidth + 1.0f) / (float)m_viewHeight;

       double angle = atan2(yPos2 - yPos1, xPos2 - xPos1);

       int angleTrans = angle * 180.0f / 3.1415926;
       float t = t2;
       if (angleTrans == 90 || angleTrans == -90) {
           t = t1;
       }

       float tsina = t / 2.0f * sin(angle);
       float tcosa = t / 2.0f * cosX(angle);

       GLfloat positions[] = {
           xPos1 + tsina, yPos1 - tcosa,
           xPos2 + tsina, yPos2 - tcosa,
           xPos2 - tsina, yPos2 + tcosa,
           xPos2 - tsina, yPos2 + tcosa,
           xPos1 - tsina, yPos1 + tcosa,
           xPos1 + tsina, yPos1 - tcosa,
       };
       glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 2 * sizeof(GLfloat), positions);
       glEnableVertexAttribArray(m_position);
       glUniform4f(m_color, color.red, color.green, color.blue, color.alpha);

       glDrawArrays(GL_TRIANGLES, 0, 6);

	   if(GL_LINE_STRIP == mode) {
		   i++;
	   }
	   else {
		   i += 2;
	   }
   }
	glDisableVertexAttribArray(m_position);
}

BEMaskProgram::BEMaskProgram() {
	m_cacheTexture = -1;
}

BEMaskProgram::~BEMaskProgram() {
	if (m_cacheTexture != -1) {
		glDeleteTextures(1, &m_cacheTexture);
	}
}

void BEMaskProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);

	m_maskTexture = glGetUniformLocation(m_program, "inputMaskTexture");
	m_maskCoordinateLocation = glGetAttribLocation(m_program, "inputTextureCoordinate");
	m_cacheTexture = -1;
}

void BEMaskProgram::draw(unsigned char* mask, int* size) {
	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, CUBE);
	glEnableVertexAttribArray(m_position);
	glVertexAttribPointer(m_maskCoordinateLocation, 2, GL_FLOAT, false, 0, TEXTURE_FLIPPED);
	glEnableVertexAttribArray(m_maskCoordinateLocation);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_cacheTexture == -1) {
		glGenTextures(1, &m_cacheTexture);
		glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size[0], size[1], 0, GL_ALPHA, GL_UNSIGNED_BYTE, mask);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size[0], size[1], 0, GL_ALPHA, GL_UNSIGNED_BYTE, mask);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
	glUniform1i(m_maskTexture, 0);

	bindData();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisable(GL_BLEND);

	glDisableVertexAttribArray(m_maskCoordinateLocation);
	glDisableVertexAttribArray(m_position);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}


void BEColorMaskProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);
	m_maskColor = glGetUniformLocation(m_program, "maskColor");
}
void BEColorMaskProgram::drawMask(unsigned char* mask, const be_rgba_color& color, int* size) {
	m_color = color;
	draw(mask, size);
}

void BEColorMaskProgram::bindData() {
	glUniform4f(m_maskColor, m_color.red, m_color.green, m_color.blue, m_color.alpha);
}

void BETextureMaskProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);

	m_maskTexture = glGetUniformLocation(m_program, "forgroundTexture");
}

void BETextureMaskProgram::drawMask(unsigned char* mask, GLuint texture, int* size) {
	m_texture = texture;
	draw(mask, size);
}

void BETextureMaskProgram::bindData() {
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glUniform1i(m_maskTexture, 1);
}

BEDrawFaceMaskProgram::BEDrawFaceMaskProgram() {
	m_faceMaskTexture = -1;
}

BEDrawFaceMaskProgram::~BEDrawFaceMaskProgram() {
	if(m_faceMaskTexture != -1) {
		glDeleteTextures(1, &m_faceMaskTexture);
	}
}

void BEAffineColorMaskProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);

	m_affine = glGetUniformLocation(m_program, "affine");
	m_maskWidth = glGetUniformLocation(m_program, "mask_width");
	m_maskHeight = glGetUniformLocation(m_program, "mask_height");
	m_viewportWidth = glGetUniformLocation(m_program, "viewport_width");
	m_viewportHeight = glGetUniformLocation(m_program, "viewport_height");
}

void BEAffineColorMaskProgram::drawMask(unsigned char* mask, const be_rgba_color& color, int* size, float* affine, int width, int height) {
	m_affineData = affine;
	m_width = width;
	m_height = height;
	m_color = color;
	memcpy(m_size, size, 2 * sizeof(int));
	draw(mask, size);
}

void BEAffineColorMaskProgram::bindData() {
	float m[9] = {
			m_affineData[0], (float)m_affineData[3], 0.0f,
		   m_affineData[1], (float)m_affineData[4], 0.0f,
			m_affineData[2], m_affineData[5] , 0.0f
	};

	glUniformMatrix3fv(m_affine, 1, GL_FALSE, m);
	glUniform4f(m_maskColor, m_color.red, m_color.green, m_color.blue, m_color.alpha);
	glUniform1f(m_maskWidth, m_size[0]);
	glUniform1f(m_maskHeight, m_size[1]);
	glUniform1f(m_viewportWidth, m_width);
	glUniform1f(m_viewportHeight, m_height);
}

BEResizeTextureProgram::BEResizeTextureProgram() {
	m_frameBuffer = -1;
	m_resizedTexture = -1;
}

BEResizeTextureProgram::~BEResizeTextureProgram() {
	if (m_frameBuffer != -1) {
		glDeleteFramebuffers(1, &m_frameBuffer);
	}
	if (m_resizedTexture != -1) {
		glDeleteTextures(1, &m_resizedTexture);
	} 
}

void BEResizeTextureProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);

	m_textureCoordinate = glGetAttribLocation(m_program, "inputTextureCoordinate");
	m_inputTexture = glGetUniformLocation(m_program, "inputImageTexture");
	glGenFramebuffers(1, &m_frameBuffer);
	glGenTextures(1, &m_resizedTexture);
}

void BEResizeTextureProgram::textureToImage(GLuint texture, unsigned char* buffer, int width, int height, GLenum format) {
	glBindTexture(GL_TEXTURE_2D, m_resizedTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);

	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_resizedTexture, 0);

	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, CUBE);
	glEnableVertexAttribArray(m_position);
	glVertexAttribPointer(m_textureCoordinate, 2, GL_FLOAT, false, 0, TEXTURE_RORATION_0);
	glEnableVertexAttribArray(m_textureCoordinate);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(m_inputTexture, 0);
	glViewport(0, 0, width, height);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_textureCoordinate);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BEResizeTextureProgram::drawTexture(GLuint texture) {
	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, CUBE);
	glEnableVertexAttribArray(m_position);
	glVertexAttribPointer(m_textureCoordinate, 2, GL_FLOAT, false, 0, TEXTURE_RORATION_0);
	glEnableVertexAttribArray(m_textureCoordinate);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(m_inputTexture, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_textureCoordinate);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
}

////////////////////////////////////////////////////////////////////////////////
BEDrawNumberTextureProgram::BEDrawNumberTextureProgram() {
	m_cacheTexture = -1;
}

BEDrawNumberTextureProgram::~BEDrawNumberTextureProgram() {
	if (m_cacheTexture != -1) {
		glDeleteTextures(1, &m_cacheTexture);
	}
}

void BEDrawNumberTextureProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	int error = glGetError();
	glUseProgram(m_program);

	m_inputTexture = glGetUniformLocation(m_program, "inputImageTexture");
	m_textureCoordLocation = glGetAttribLocation(m_program, "inputTextureCoordinate");
	m_cacheTexture = -1;
}


void BEDrawNumberTextureProgram::drawNumber(bef_ai_rectf& rect, unsigned char* image, int width, int height) {
	float x1 = rect.left;
	float y1 = rect.top;
	float x2 = rect.right;
	float y2 = rect.bottom;

	GLfloat positions[] = {
		x1, y2,
		x2, y2,
		x1, y1,
		x2, y1
	};

	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, positions);
	glEnableVertexAttribArray(m_position);
	glVertexAttribPointer(m_textureCoordLocation, 2, GL_FLOAT, false, 0, TEXTURE_FLIPPED);
	glEnableVertexAttribArray(m_textureCoordLocation);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_cacheTexture == -1) {
		glGenTextures(1, &m_cacheTexture);
		glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_cacheTexture);
	glUniform1i(m_inputTexture, 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_textureCoordLocation);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////

void BEDrawFaceMaskProgram::initialize(const char* vertext, const char* fragment) {
	__super::initialize(vertext, fragment);
	glUseProgram(m_program);

	m_faceMaskTextureCoordinate = glGetAttribLocation(m_program, "inputTextureCoordinate");
	m_faceMaskSeg = glGetUniformLocation(m_program, "inputMask");
	m_faceMaskWrapMat = glGetUniformLocation(m_program, "warpMat");
	m_faceMaskColor = glGetUniformLocation(m_program, "maskColor");
}

void BEDrawFaceMaskProgram::drawSegment(unsigned char* mask, float* affine, const be_rgba_color& color, GLuint texture, int* size, int width, int height) {
	float maskWidth = size[0] * 1.0f;
	float maskHeight = size[1] * 1.0f;

	glUseProgram(m_program);
	glVertexAttribPointer(m_position, 2, GL_FLOAT, false, 0, CUBE);
	glEnableVertexAttribArray(m_position);

	glVertexAttribPointer(m_faceMaskTextureCoordinate, 2, GL_FLOAT, false, 0, TEXTURE_FLIPPED);
	glEnableVertexAttribArray(m_faceMaskTextureCoordinate);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float scaleX = width / maskWidth;
	float scaleY = height / maskHeight;
	if (m_faceMaskWrapMat >= 0) {
		float m[16] = {
			affine[0] * scaleX, affine[3] * scaleX, 0.0f, 0.0f,
			affine[1] * scaleY, affine[4] * scaleY, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			affine[2] / maskWidth,  affine[5] / maskHeight, 0.0f, 1.0f
		};
		glUniformMatrix4fv(m_faceMaskWrapMat, 1, GL_FALSE, m);
	}

	glUniform3f(m_faceMaskColor, color.red, color.green, color.blue);

	glActiveTexture(GL_TEXTURE0);
	if (m_faceMaskTexture == -1) {
		glGenTextures(1, &m_faceMaskTexture);
		glBindTexture(GL_TEXTURE_2D, m_faceMaskTexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size[0], size[1], 0, GL_ALPHA, GL_UNSIGNED_BYTE, mask);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_faceMaskTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size[0], size[1], 0, GL_ALPHA, GL_UNSIGNED_BYTE, mask);
	}
	glUniform1i(m_faceMaskSeg, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisable(GL_BLEND);
	glDisableVertexAttribArray(m_position);
	glDisableVertexAttribArray(m_faceMaskTextureCoordinate);

	glBindTexture(GL_TEXTURE_2D, 0);
}
