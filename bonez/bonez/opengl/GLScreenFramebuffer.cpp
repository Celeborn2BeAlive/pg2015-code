#include "GLScreenFramebuffer.hpp"

#include <stdexcept>

namespace BnZ {

const Vec4u GLScreenFramebuffer::NULL_OBJECT_ID = Vec4u(0u);

bool GLScreenFramebuffer::init(size_t width, size_t height) {
    m_nWidth = width;
    m_nHeight = height;

    m_ColorTexture.setImage(0, GL_RGBA32F, width, height, 0,
                               GL_RGBA, GL_FLOAT, nullptr);
    m_ColorTexture.setMinFilter(GL_NEAREST);
    m_ColorTexture.setMagFilter(GL_NEAREST);

    glNamedFramebufferTexture2DEXT(m_FBO.glId(), GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, m_ColorTexture.glId(), 0);

    m_ObjectIDTexture.setImage(0, GL_RGBA32UI, width, height, 0,
                               GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
    m_ObjectIDTexture.setMinFilter(GL_NEAREST);
    m_ObjectIDTexture.setMagFilter(GL_NEAREST);

    glNamedFramebufferTexture2DEXT(m_FBO.glId(), GL_COLOR_ATTACHMENT1,
                                 GL_TEXTURE_2D, m_ObjectIDTexture.glId(), 0);

    m_DepthTexture.setImage(0, GL_DEPTH_COMPONENT32F, width, height, 0,
                            GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    m_DepthTexture.setMinFilter(GL_NEAREST);
    m_DepthTexture.setMagFilter(GL_NEAREST);

    glNamedFramebufferTexture2DEXT(m_FBO.glId(), GL_DEPTH_ATTACHMENT,
                                 GL_TEXTURE_2D, m_DepthTexture.glId(), 0);

    m_DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    m_DrawBuffers[1] = GL_COLOR_ATTACHMENT1;

    // check that the FBO is complete
    GLenum status = glCheckNamedFramebufferStatusEXT(m_FBO.glId(), GL_DRAW_FRAMEBUFFER);
    if(GL_FRAMEBUFFER_COMPLETE != status) {
        std::cerr << GLFramebufferErrorString(status) << std::endl;
        throw std::runtime_error(GLFramebufferErrorString(status));
    }

    return true;
}

Vec4u GLScreenFramebuffer::getObjectID(const Vec2u& pixel) const {
    bindForReading();
    setReadBuffer(1u);

    Vec4u value;
    glReadPixels(pixel.x, pixel.y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, value_ptr(value));

    glReadBuffer(GL_NONE);

    return value;
}

}
