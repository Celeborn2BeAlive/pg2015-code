#pragma once

#include "utils/GLFramebuffer.hpp"

namespace BnZ {

// The screen framebuffer holds two color buffers:
//  - the final color renderer on screen
//  - the objectID allowing picking
class GLScreenFramebuffer {
public:
    static const Vec4u NULL_OBJECT_ID;

    bool init(size_t width, size_t height);

    bool init(const Vec2u& size) {
        return init(size.x, size.y);
    }

    void bindForDrawing() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO.glId());

        // Specify the outputs of the fragment shader
        glDrawBuffers(2, m_DrawBuffers);
    }

    void bindForReading() const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO.glId());
    }

    void setReadBuffer(uint32_t index) const {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
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

    const GLTexture2D& getColorBuffer() const {
        return m_ColorTexture;
    }

    GLTexture2D& getColorBuffer() {
        return m_ColorTexture;
    }

    template<uint32_t SrcColorBufferCount, bool hasDepthBuffer>
    void blitFramebuffer(const GLFramebuffer2D<SrcColorBufferCount, hasDepthBuffer>& src, GLbitfield mask, GLenum filter) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO.glId());
        src.bindForReading();

        glBlitFramebuffer(0, 0, src.getWidth(), src.getHeight(),
                          0, 0, getWidth(), getHeight(),
                          mask, filter);
    }

    void blitOnDefaultFramebuffer(const Vec4u& viewport, GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        bindForReading();
        setReadBuffer(0);

        glBlitFramebuffer(0, 0, getWidth(), getHeight(),
                          viewport.x, viewport.y, viewport.x + viewport.z, viewport.y + viewport.w,
                          mask, filter);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    // Return the object ID stored for a pixel
    Vec4u getObjectID(const Vec2u& pixel) const;

    uint32_t addObjectType() {
        return m_nNextObjectID++;
    }

    void setGLViewport() const {
        glViewport(0, 0, getWidth(), getHeight());
    }

private:
    size_t m_nWidth = 0u, m_nHeight = 0u;
    GLFramebufferObject m_FBO;
    GLTexture2D m_ColorTexture;
    GLTexture2D m_ObjectIDTexture;
    GLTexture2D m_DepthTexture;
    GLenum m_DrawBuffers[2];

    uint32_t m_nNextObjectID = 1u;
};

}
