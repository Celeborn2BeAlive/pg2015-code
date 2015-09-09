#pragma once

#include <bonez/opengl/opengl.hpp>
#include <iostream>
#include <cassert>

#include "GLObject.hpp"

namespace BnZ {

inline void activeTextureUnit(GLuint index) {
    glActiveTexture(GL_TEXTURE0 + index);
}

template<GLenum textureType>
class GLTextureBase : GLTextureObject<textureType> {
protected:
    GLTextureBase() = default;

    GLTextureBase(GLTextureBase&&) = default;

    GLTextureBase& operator =(GLTextureBase&&) = default;

    ~GLTextureBase() = default;
public:
    static const GLenum TEXTURE_TYPE = textureType;

    using GLTextureObject<textureType>::glId;

    void getImage(GLint level, GLenum format, GLenum type, GLvoid *pixels) const {
        glGetTextureImageEXT(glId(), TEXTURE_TYPE, level, format, type, pixels);
    }

    void setParameter(GLenum pname, GLfloat param) {
        glTextureParameterfEXT(glId(), TEXTURE_TYPE, pname, param);
    }

    void setParameter(GLenum pname, GLint param) {
        glTextureParameteriEXT(glId(), TEXTURE_TYPE, pname, param);
    }

    void setMinFilter(GLint param) {
        setParameter(GL_TEXTURE_MIN_FILTER, param);
    }

    void setMagFilter(GLint param) {
        setParameter(GL_TEXTURE_MAG_FILTER, param);
    }

    void setFilters(GLint minFilter, GLint magFilter) {
        setMinFilter(minFilter);
        setMagFilter(magFilter);
    }

    void setWrapS(GLint param) {
        setParameter(GL_TEXTURE_WRAP_S, param);
    }

    void setWrapT(GLint param) {
        setParameter(GL_TEXTURE_WRAP_T, param);
    }

    void setWrapR(GLint param) {
        setParameter(GL_TEXTURE_WRAP_R, param);
    }

    void generateMipmap() {
        glGenerateTextureMipmapEXT(glId(), textureType);
    }

    void setCompareMode(GLint param) {
        setParameter(GL_TEXTURE_COMPARE_MODE, param);
    }

    void setCompareFunc(GLint param) {
        setParameter(GL_TEXTURE_COMPARE_FUNC, param);
    }

    void clear(GLint level, GLenum format, GLenum type, const void* data) {
        glClearTexImage(glId(), level, format, type, data);
    }

    void bind() const {
        glBindTexture(TEXTURE_TYPE, glId());
    }

    void bind(GLuint textureUnitIndex) const {
        activeTextureUnit(textureUnitIndex);
        bind();
    }

    void bindImage(GLuint unit, GLint level, GLenum access, GLenum format) const {
        glBindImageTexture(unit, glId(), level, GL_TRUE, 0, access, format);
    }
};

template<GLenum textureType>
class GLMultiLayerTexture: public GLTextureBase<textureType> {
protected:
    GLMultiLayerTexture() = default;

    GLMultiLayerTexture(GLMultiLayerTexture&&) = default;

    GLMultiLayerTexture& operator =(GLMultiLayerTexture&&) = default;

    ~GLMultiLayerTexture() = default;
public:
    using GLTextureBase<textureType>::bindImage;

    void bindImage(GLuint unit, GLint level, GLint layer, GLenum access, GLenum format) const {
        glBindImageTexture(unit, GLTextureBase<textureType>::glId(), level, GL_FALSE, layer, access, format);
    }
};

class GLTexture2D: public GLTextureBase<GL_TEXTURE_2D> {
    GLsizei m_nWidth = 0;
    GLsizei m_nHeight = 0;
public:
    void setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
        GLint border, GLenum format, GLenum type, const GLvoid* data) {
        glTextureImage2DEXT(glId(), GL_TEXTURE_2D, level, internalFormat, width, height,
            border, format, type, data);
        m_nWidth = width;
        m_nHeight = height;
    }

    void setSubImage(GLint level,
        GLint xoffset,
        GLint yoffset,
        GLsizei width,
        GLsizei height,
        GLenum format,
        GLenum type,
        const GLvoid * data) {
        glTextureSubImage2DEXT(glId(), GL_TEXTURE_2D, level, xoffset, yoffset, width, height,
            format, type, data);
    }

    void setSubImage(
        GLint level,
        GLenum format,
        GLenum type,
        const GLvoid * data) {
        setSubImage(level, 0, 0, m_nWidth, m_nHeight, format, type, data);
    }

    void setStorage(GLint levels, GLenum internalFormat, GLsizei width, GLsizei height) {
        glTextureStorage2DEXT(glId(), GL_TEXTURE_2D, levels, internalFormat, width, height);
        m_nWidth = width;
        m_nHeight = height;
    }

    GLsizei getWidth() const {
        return m_nWidth;
    }

    GLsizei getHeight() const {
        return m_nHeight;
    }
};

class GLTexture3D: public GLMultiLayerTexture<GL_TEXTURE_3D> {
    GLsizei m_nWidth = 0;
    GLsizei m_nHeight = 0;
    GLsizei m_nDepth = 0;
public:
    void setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
        GLint border, GLenum format, GLenum type, const GLvoid* data) {
        glTextureImage3DEXT(glId(), GL_TEXTURE_3D, level, internalFormat, width, height, depth,
            border, format, type, data);
        m_nWidth = width;
        m_nHeight = height;
        m_nDepth = depth;
    }

    void setStorage(GLint levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth) {
        glTextureStorage3DEXT(glId(), GL_TEXTURE_3D, levels, internalFormat, width, height, depth);
        m_nWidth = width;
        m_nHeight = height;
        m_nDepth = depth;
    }

    GLsizei getWidth() const {
        return m_nWidth;
    }

    GLsizei getHeight() const {
        return m_nHeight;
    }

    GLsizei getDepth() const {
        return m_nDepth;
    }
};

class GLTextureCubeMap: public GLMultiLayerTexture<GL_TEXTURE_CUBE_MAP> {
public:
    void setImage(GLint face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
        GLint border, GLenum format, GLenum type, const GLvoid* data) {
        glTextureImage2DEXT(glId(), GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, internalFormat, width, height,
            border, format, type, data);
    }

    void setStorage(GLint face, GLint levels, GLenum internalFormat, GLsizei width, GLsizei height) {
        glTextureStorage2DEXT(glId(), GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, levels, internalFormat, width, height);
    }

    void setStorage(GLint levels, GLenum internalFormat, GLsizei width, GLsizei height) {
        glTextureStorage2DEXT(glId(), GL_TEXTURE_CUBE_MAP, levels, internalFormat, width, height);
    }
};

class GLTexture2DArray: public GLMultiLayerTexture<GL_TEXTURE_2D_ARRAY> {
    GLsizei m_nWidth = 0;
    GLsizei m_nHeight = 0;
    GLsizei m_nLayerCount = 0;
public:
    void setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
        GLsizei nbLayers, GLint border, GLenum format, GLenum type, const GLvoid* data) {
        glTextureImage3DEXT(glId(), GL_TEXTURE_2D_ARRAY, level, internalFormat, width, height, nbLayers,
            border, format, type, data);
        m_nWidth = width;
        m_nHeight = height;
        m_nLayerCount = nbLayers;
    }

    void setSubImage(GLint level,
        GLint xoffset,
        GLint yoffset,
        GLint zoffset,
        GLsizei width,
        GLsizei height,
        GLsizei nbLayers,
        GLenum format,
        GLenum type,
        const GLvoid * data) {
        glTextureSubImage3DEXT(glId(), GL_TEXTURE_2D_ARRAY, level, xoffset, yoffset, zoffset, width, height, nbLayers,
            format, type, data);
    }

    void setSubImage(
        GLint level,
        GLenum format,
        GLenum type,
        const GLvoid * data) {
        setSubImage(level, 0, 0, 0, m_nWidth, m_nHeight, m_nLayerCount, format, type, data);
    }

    void setStorage(GLint levels, GLenum internalFormat, GLsizei width, GLsizei height,
        GLsizei nbLayers) {
        glTextureStorage3DEXT(glId(), GL_TEXTURE_2D_ARRAY, levels, internalFormat, width, height, nbLayers);
        m_nWidth = width;
        m_nHeight = height;
        m_nLayerCount = nbLayers;
    }

    GLsizei getWidth() const {
        return m_nWidth;
    }

    GLsizei getHeight() const {
        return m_nHeight;
    }

    GLsizei getLayerCount() const {
        return m_nLayerCount;
    }
};

class GLTextureCubeMapArray: public GLMultiLayerTexture<GL_TEXTURE_CUBE_MAP_ARRAY> {
    GLsizei m_nWidth = 0;
    GLsizei m_nHeight = 0;
    GLsizei m_nLayerCount = 0;
public:

    // data must contain 6 * nbLayers images
    void setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
        GLsizei nbLayers, GLint border, GLenum format, GLenum type, const GLvoid* data) {
        glTextureImage3DEXT(glId(), GL_TEXTURE_CUBE_MAP_ARRAY, level, internalFormat, width, height, 6 * nbLayers,
            border, format, type, data);
        m_nWidth = width;
        m_nHeight = height;
        m_nLayerCount = nbLayers;
    }

    // The total depth of the underlying cube map array is 6 * nbLayers
    void setStorage(GLint levels, GLenum internalFormat, GLsizei width, GLsizei height,
        GLsizei nbLayers) {
        glTextureStorage3DEXT(glId(), GL_TEXTURE_CUBE_MAP_ARRAY, levels, internalFormat, width, height, 6 * nbLayers);
        m_nWidth = width;
        m_nHeight = height;
        m_nLayerCount = nbLayers;
    }

    GLsizei getWidth() const {
        return m_nWidth;
    }

    GLsizei getHeight() const {
        return m_nHeight;
    }

    GLsizei getLayerCount() const {
        return m_nLayerCount;
    }

    GLsizei getLayerFaceCount() const {
        return 6 * m_nLayerCount;
    }
};

}
