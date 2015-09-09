#pragma once

#include "GLObject.hpp"
#include "GLTexture.hpp"

namespace BnZ {

template<uint32_t NbColorBuffers>
class GLCubeFramebuffer {
public:
    static const uint32_t COLORBUFFER_COUNT = NbColorBuffers;

    bool init(size_t res, const GLenum* internalFormats,
              GLenum depthInternalFormat) {
        m_nRes = res;

        // Allocation of the textures for each attribute and attachment to the FBO
        for(uint32_t i = 0; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i].setStorage(1, internalFormats[i], m_nRes, m_nRes);
            m_ColorBuffers[i].setMinFilter(GL_LINEAR);
            m_ColorBuffers[i].setMagFilter(GL_LINEAR);
            m_ColorBuffers[i].setWrapS(GL_CLAMP_TO_EDGE);
            m_ColorBuffers[i].setWrapT(GL_CLAMP_TO_EDGE);

            m_ColorBuffers[i].makeTextureHandleResident();

            glNamedFramebufferTextureEXT(m_Fbo.glId(), GL_COLOR_ATTACHMENT0 + i,
                                         m_ColorBuffers[i].glId(), 0);

            // Add corresponding draw buffer GL constant
            m_DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }

        // Allocation and attachment of depth texture
        m_DepthBuffer.setStorage(1, depthInternalFormat, m_nRes, m_nRes);
        m_DepthBuffer.setMinFilter(GL_LINEAR);
        m_DepthBuffer.setMagFilter(GL_LINEAR);
        m_DepthBuffer.setWrapS(GL_CLAMP_TO_EDGE);
        m_DepthBuffer.setWrapT(GL_CLAMP_TO_EDGE);

        glNamedFramebufferTextureEXT(m_Fbo.glId(), GL_DEPTH_ATTACHMENT,
                                     m_DepthBuffer.glId(), 0);

        // check that the FBO is complete
        GLenum status = glCheckNamedFramebufferStatusEXT(m_Fbo.glId(), GL_DRAW_FRAMEBUFFER);
        if(GL_FRAMEBUFFER_COMPLETE != status) {
            std::cerr << GLFramebufferErrorString(status) << std::endl;
            return false;
        }

        return true;
    }

    void bindForDrawing() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Fbo.glId());

        // Specify the outputs of the fragment shader
        glDrawBuffers(COLORBUFFER_COUNT, m_DrawBuffers);
    }

    void bindForReading() const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_Fbo.glId());
    }

    void setReadBuffer(uint32_t index) const {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
    }

    const GLTextureCubeMap& getColorBuffer(uint32_t index) const {
        return m_ColorBuffers[index];
    }

    GLTextureCubeMap& getColorBuffer(uint32_t index) {
        return m_ColorBuffers[index];
    }

    const GLTextureCubeMap& getDepthBuffer() const {
        return m_DepthBuffer;
    }

    GLTextureCubeMap& getDepthBuffer() {
        return m_DepthBuffer;
    }

    size_t getResolution() const {
        return m_nRes;
    }

    GLCubeFramebuffer() = default;

    GLCubeFramebuffer(GLCubeFramebuffer&& rvalue) :
        m_Fbo(std::move(rvalue.m_Fbo)),
        m_DepthBuffer(std::move(rvalue.m_DepthBuffer)),
        m_nRes(rvalue.m_nRes) {
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i] = std::move(rvalue.m_ColorBuffers[i]);
        }
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_DrawBuffers[i] = std::move(rvalue.m_DrawBuffers[i]);
        }
    }

    GLCubeFramebuffer& operator =(GLCubeFramebuffer&& rvalue) {
        m_Fbo = std::move(rvalue.m_Fbo);
        m_DepthBuffer = std::move(rvalue.m_DepthBuffer);
        m_nRes = rvalue.m_nRes;
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_ColorBuffers[i] = std::move(rvalue.m_ColorBuffers[i]);
        }
        for (auto i = 0u; i < COLORBUFFER_COUNT; ++i) {
            m_DrawBuffers[i] = std::move(rvalue.m_DrawBuffers[i]);
        }
        return *this;
    }

private:
    GLFramebufferObject m_Fbo;
    GLTextureCubeMap m_ColorBuffers[COLORBUFFER_COUNT];
    GLTextureCubeMap m_DepthBuffer;

    GLenum m_DrawBuffers[COLORBUFFER_COUNT];

    size_t m_nRes = 0;
};

}
