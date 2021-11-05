//
//  PBOReader.hpp
//  BytedEffects
//
//  Created by wudan on 2020/3/19.
//  Copyright © 2020 Bytedance. All rights reserved.
//

#ifndef PBOReader_hpp
#define PBOReader_hpp

#include <QObject>

//#include "glExtension.h"

#include <iostream>
#include <thread>
#include <memory>

#define GL_SILENCE_DEPRECATION


class PBOReader {
    static constexpr int DATA_SIZE = 1920*1080*4;
//    static constexpr int DATA_SIZE = 720*1280*4;
public:
    class DisposableBuffer {
        friend class PBOReader;
    public:
        DisposableBuffer(const DisposableBuffer&) = delete;
        DisposableBuffer& operator=(DisposableBuffer const&) = delete;
        DisposableBuffer& operator=(DisposableBuffer&& o) {
            _id = o._id;
            _buffer = o._buffer;
            o._id = 0;
            o._buffer = nullptr;
            return *this;
        }
        
        DisposableBuffer() {
            _id = 0;
            _buffer = nullptr;
        }
        
        GLubyte* get() {
            return _buffer;
        }

        ~DisposableBuffer() {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, _id);
            if (_buffer) {
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    private:
        explicit DisposableBuffer(GLuint pboId, int size) {
            _id = pboId;
            glBindBuffer(GL_PIXEL_PACK_BUFFER, _id);
            //_buffer = (GLubyte*)glMapBufferOES(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY); 
            _buffer = (GLubyte*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT);
        }
        
        GLuint _id;
        GLubyte* _buffer;
    };
    
    PBOReader(int dataSize) {
        m_dataSize = dataSize;
        //glExtension& ext = glExtension::getInstance();
        //pboSupported = ext.isSupported("GL_ARB_pixel_buffer_object");
        if(pboSupported)
        {
            std::cout << "Video card supports GL_ARB_pixel_buffer_object." << std::endl;
        }
        else
        {
            std::cout << "[ERROR] Video card does not supports GL_ARB_pixel_buffer_object." << std::endl;
        }
        
        if(pboSupported)
        {
            // create 2 pixel buffer objects, you need to delete them when program exits.
            // glBufferData() with NULL pointer reserves only memory space.
            glGenBuffers(2, pboIds);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[0]);
            glBufferData(GL_PIXEL_PACK_BUFFER, m_dataSize, 0, GL_STREAM_READ);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[1]);
            glBufferData(GL_PIXEL_PACK_BUFFER, m_dataSize, 0, GL_STREAM_READ);
    
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        index = 0;
        fbo = 0;
    }
    
    ~PBOReader() {
       
    }

    void release()
    {
        if (pboSupported)
        {
            glDeleteBuffers(2, pboIds);
        }

        if (fbo != 0) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }

    bool read(GLuint texture, int width, int height, DisposableBuffer& readback) {
        GLuint prevFbo;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&prevFbo);
        if (fbo == 0) {
            glGenFramebuffers(1, &fbo);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        bool ret = read(width, height, readback);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);

        return ret;
    }
    
    bool read(int width, int height, DisposableBuffer& readback) {
        if (!pboSupported) {
            std::cout << "[ERROR] Video card does not supports GL_ARB_pixel_buffer_object." << std::endl;
            return false;
        }
        
        index = (index + 1) % 2;
        int nextIndex = (index + 1) % 2;
        
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[index]);
        //kickoff aysnc read, this call return immediately
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        
        DisposableBuffer buffer(pboIds[nextIndex], width*height*4);
        readback = std::move(buffer);
        if (!readback.get()) {
            return false;
        }
        
        return true;
    }

private:
    GLuint pboIds[2];
    GLuint fbo;
    int index;
    bool pboSupported = true;
    int m_dataSize = 1920 * 1080 * 4;
};

#endif /* PBOReader_hpp */
