#pragma once

#include "GLTexture.hpp"

namespace BnZ {

template<GLenum textureType>
class GLTextureHandle {
    GLuint64 m_nHandle = 0u;
public:
    explicit GLTextureHandle(GLuint64 handle = 0u) : m_nHandle(handle) {
    }

    explicit GLTextureHandle(const GLTextureBase<textureType>& texture):
        m_nHandle(glGetTextureHandleNV(texture.glId())) {
    }

    explicit operator bool() const {
        return m_nHandle != 0u;
    }

    explicit operator GLuint64() const {
        return m_nHandle;
    }

    bool isResident() const {
        assert(m_nHandle);
        return glIsTextureHandleResidentNV(m_nHandle);
    }

    void makeResident() const {
        assert(m_nHandle);
        return glMakeTextureHandleResidentNV(m_nHandle);
    }

    void makeNonResident() const {
        assert(m_nHandle);
        return glMakeTextureHandleNonResidentNV(m_nHandle);
    }
};

template<GLenum textureType>
class GLImageHandle {
    GLuint64 m_nHandle = 0u;
public:
    explicit GLImageHandle(GLuint64 handle = 0u) : m_nHandle(handle) {
    }

    explicit GLImageHandle(const GLTextureBase<textureType>& texture,
                             GLint level,
                             GLint layer,
                             GLenum format):
        m_nHandle(glGetImageHandleNV(texture.glId(), level, GL_FALSE, layer, format)) {
    }

    explicit GLImageHandle(const GLTextureBase<textureType>& texture,
                             GLint level,
                             GLenum format):
        m_nHandle(glGetImageHandleNV(texture.glId(), level, GL_TRUE, 0, format)) {
    }

    explicit operator bool() const {
        return m_nHandle != 0u;
    }

    explicit operator GLuint64() const {
        return m_nHandle;
    }

    void makeResident(GLenum access) const {
        glMakeImageHandleResidentNV(m_nHandle, access);
    }

    bool isResident() const {
        return glIsImageHandleResidentNV(m_nHandle);
    }

    void makeNonResident() const {
        glMakeImageHandleNonResidentNV(m_nHandle);
    }
};

using GLTexture2DHandle = GLTextureHandle<GL_TEXTURE_2D>;
using GLTexture3DHandle = GLTextureHandle<GL_TEXTURE_3D>;
using GLTextureCubeMapHandle = GLTextureHandle<GL_TEXTURE_CUBE_MAP>;
using GLTexture2DArrayHandle = GLTextureHandle<GL_TEXTURE_2D_ARRAY>;
using GLTextureCubeMapArrayHandle = GLTextureHandle<GL_TEXTURE_CUBE_MAP_ARRAY>;

using GLImage2DHandle = GLImageHandle<GL_TEXTURE_2D>;
using GLImage3DHandle = GLImageHandle<GL_TEXTURE_3D>;
using GLImageCubeMapHandle = GLImageHandle<GL_TEXTURE_CUBE_MAP>;
using GLImage2DArrayHandle = GLImageHandle<GL_TEXTURE_2D_ARRAY>;
using GLImageCubeMapArrayHandle = GLImageHandle<GL_TEXTURE_CUBE_MAP_ARRAY>;

}
