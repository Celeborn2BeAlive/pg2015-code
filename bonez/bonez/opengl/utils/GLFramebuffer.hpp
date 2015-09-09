#pragma once

#include "GLObject.hpp"
#include "GLTexture.hpp"
#include "GLState.hpp"

#include <bonez/types.hpp>

namespace BnZ {

inline const char* GLFramebufferErrorString(GLenum error) {
    switch(error) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE: the framebuffer is complete.";
    case GL_FRAMEBUFFER_UNDEFINED:
        return "GL_FRAMEBUFFER_UNDEFINED: the default framebuffer does not exist.";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: any of the framebuffer attachment "
        "points are framebuffer incomplete.";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: the framebuffer does not "
        "have at least one image attached to it.";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: the value of"
        "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for any color attachment point(s) "
        "named by GL_DRAWBUFFERi.";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: GL_READ_BUFFER is not GL_NONE and "
        "the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment "
        "point named by GL_READ_BUFFER.";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED: the combination of internal formats of the attached "
        "images violates an implementation-dependent set of restrictions.";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: the value of GL_RENDERBUFFER_SAMPLES is "
        "not the same for all attached renderbuffers; OR the value of GL_TEXTURE_SAMPLES is the "
        "not same for all attached textures; OR the attached images are a mix of renderbuffers "
        "and textures, the value of GL_RENDERBUFFER_SAMPLES does not match the value of GL_TEXTURE_SAMPLES. "
        "OR the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not the same for all attached textures; "
        "OR the attached images are a mix of renderbuffers and textures, the value of "
        "GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not GL_TRUE for all attached textures.";
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: any framebuffer attachment is layered, "
        "and any populated attachment is not layered, or if all populated color attachments are not "
        "from textures of the same target.";
    }

    return "";
}

template<uint32_t NbColorBuffers>
class GLFramebuffer2DBase {
public:
    static const uint32_t COLORBUFFER_COUNT = NbColorBuffers;

    template<uint32_t SrcColorBufferCount>
    void blitFramebuffer(const GLFramebuffer2DBase<SrcColorBufferCount>& src, GLbitfield mask, GLenum filter) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Fbo.glId());
        src.bindForReading();

        glBlitFramebuffer(0, 0, src.getWidth(), src.getHeight(),
                          0, 0, getWidth(), getHeight(),
                          mask, filter);
    }

    void bindForDrawing() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Fbo.glId());

        // Specify the outputs of the fragment shader
        glDrawBuffers(COLORBUFFER_COUNT, m_DrawBuffers);
    }

    void setDrawingViewport() {
        glViewport(0, 0, m_nWidth, m_nHeight);
    }

    void bindForReading() const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_Fbo.glId());
    }

    void setReadBuffer(uint32_t index) const {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    }

    const GLTexture2D& getColorBuffer(uint32_t index) const {
        return m_ColorBuffers[index];
    }

    GLTexture2D& getColorBuffer(uint32_t index) {
        return m_ColorBuffers[index];
    }

    size_t getWidth() const {
        return m_nWidth;
    }

    size_t getHeight() const {
        return m_nHeight;
    }

    Vec2u getSize() const {
        return Vec2u(getWidth(), getHeight());
    }

protected:
    void init(size_t width, size_t height, const GLenum internalFormats[NbColorBuffers], GLenum filter) {
        m_nWidth = width;
        m_nHeight = height;

        // Allocation of the textures for each attribute and attachment to the FBO
        for(uint32_t i = 0; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i] = GLTexture2D();
            m_ColorBuffers[i].setStorage(1, internalFormats[i], width, height);
            m_ColorBuffers[i].setMinFilter(filter);
            m_ColorBuffers[i].setMagFilter(filter);

            glNamedFramebufferTexture2DEXT(m_Fbo.glId(), GL_COLOR_ATTACHMENT0 + i,
                                         GL_TEXTURE_2D, m_ColorBuffers[i].glId(), 0);

            // Add corresponding draw buffer GL constant
            m_DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
    }

    GLFramebuffer2DBase() = default;

    GLFramebuffer2DBase(GLFramebuffer2DBase&& rvalue) :
        m_Fbo(std::move(rvalue.m_Fbo)),
        m_nWidth(rvalue.m_nWidth), m_nHeight(rvalue.m_nHeight) {
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i] = std::move(rvalue.m_ColorBuffers[i]);
        }
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_DrawBuffers[i] = std::move(rvalue.m_DrawBuffers[i]);
        }
    }

    GLFramebuffer2DBase& operator =(GLFramebuffer2DBase&& rvalue) {
        m_Fbo = std::move(rvalue.m_Fbo);
        m_nWidth = rvalue.m_nWidth;
        m_nHeight = rvalue.m_nHeight;
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i] = std::move(rvalue.m_ColorBuffers[i]);
        }
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_DrawBuffers[i] = std::move(rvalue.m_DrawBuffers[i]);
        }
        return *this;
    }

    GLFramebufferObject m_Fbo;
    GLTexture2D m_ColorBuffers[COLORBUFFER_COUNT];

    GLenum m_DrawBuffers[COLORBUFFER_COUNT];

    size_t m_nWidth = 0, m_nHeight = 0;
};

template<uint32_t NbColorBuffers, bool hasDepthBuffer = true>
class GLFramebuffer2D: public GLFramebuffer2DBase<NbColorBuffers> {
    using ParentClass = GLFramebuffer2DBase<NbColorBuffers>;
public:
    static const uint32_t COLORBUFFER_COUNT = NbColorBuffers;
    static const bool HAS_DEPTH_BUFFER = hasDepthBuffer;

    bool init(size_t width, size_t height, const GLenum internalFormats[NbColorBuffers],
              GLenum depthInternalFormat, GLenum filter = GL_NEAREST) {
        ParentClass::init(width, height, internalFormats, filter);

        // Allocation and attachment of depth texture
        m_DepthBuffer = GLTexture2D();
        m_DepthBuffer.setStorage(1, depthInternalFormat, width, height);
        m_DepthBuffer.setMinFilter(filter);
        m_DepthBuffer.setMagFilter(filter);

        glNamedFramebufferTexture2DEXT(ParentClass::m_Fbo.glId(), GL_DEPTH_ATTACHMENT,
                                     GL_TEXTURE_2D, m_DepthBuffer.glId(), 0);

        // check that the FBO is complete
        GLenum status = glCheckNamedFramebufferStatusEXT(ParentClass::m_Fbo.glId(), GL_DRAW_FRAMEBUFFER);
        if(GL_FRAMEBUFFER_COMPLETE != status) {
            std::cerr << GLFramebufferErrorString(status) << std::endl;
            return false;
        }

        return true;
    }

    bool init(const Vec2u& size, const GLenum internalFormats[NbColorBuffers],
              GLenum depthInternalFormat, GLenum filter = GL_NEAREST) {
        return init(size.x, size.y, internalFormats, depthInternalFormat, filter);
    }

    bool init(size_t width, size_t height, const std::array<GLenum, NbColorBuffers>& internalFormats,
              GLenum depthInternalFormat, GLenum filter = GL_NEAREST) {
        return init(width, height, internalFormats.data(), depthInternalFormat, filter);
    }

    bool init(const Vec2u& size, const std::array<GLenum, NbColorBuffers>& internalFormats,
              GLenum depthInternalFormat, GLenum filter = GL_NEAREST) {
        return init(size.x, size.y, internalFormats, depthInternalFormat, filter);
    }

    const GLTexture2D& getDepthBuffer() const {
        return m_DepthBuffer;
    }

    GLTexture2D& getDepthBuffer() {
        return m_DepthBuffer;
    }

    GLFramebuffer2D() = default;

    GLFramebuffer2D(GLFramebuffer2D&& rvalue) : ParentClass(rvalue),
        m_DepthBuffer(std::move(rvalue.m_DepthBuffer)) {
    }

    GLFramebuffer2D& operator =(GLFramebuffer2D&& rvalue) {
        ParentClass::operator =(rvalue);
        m_DepthBuffer = std::move(rvalue.m_DepthBuffer);
        return *this;
    }

private:
    GLTexture2D m_DepthBuffer;
};

template<uint32_t NbColorBuffers>
class GLFramebuffer2D<NbColorBuffers, false>: public GLFramebuffer2DBase<NbColorBuffers> {
    using ParentClass = GLFramebuffer2DBase<NbColorBuffers>;
public:
    static const uint32_t COLORBUFFER_COUNT = NbColorBuffers;
    static const bool HAS_DEPTH_BUFFER = false;

    bool init(size_t width, size_t height, const GLenum internalFormats[NbColorBuffers], GLenum filter = GL_NEAREST) {
        ParentClass::init(width, height, internalFormats, filter);

        // check that the FBO is complete
        GLenum status = glCheckNamedFramebufferStatusEXT(ParentClass::m_Fbo.glId(), GL_DRAW_FRAMEBUFFER);
        if(GL_FRAMEBUFFER_COMPLETE != status) {
            std::cerr << GLFramebufferErrorString(status) << std::endl;
            return false;
        }

        return true;
    }

    bool init(const Vec2u& size, const GLenum internalFormats[NbColorBuffers], GLenum filter = GL_NEAREST) {
        return init(size.x, size.y, internalFormats, filter);
    }

    bool init(size_t width, size_t height, const std::array<GLenum, NbColorBuffers>& internalFormats, GLenum filter = GL_NEAREST) {
        return init(width, height, internalFormats.data(), filter);
    }

    bool init(const Vec2u& size, const std::array<GLenum, NbColorBuffers>& internalFormats, GLenum filter = GL_NEAREST) {
        return init(size.x, size.y, internalFormats, filter);
    }

    GLFramebuffer2D() = default;

    GLFramebuffer2D(GLFramebuffer2D&& rvalue) : ParentClass(rvalue) {
    }

    GLFramebuffer2D& operator =(GLFramebuffer2D&& rvalue) {
        ParentClass::operator =(rvalue);
        return *this;
    }
};

}
