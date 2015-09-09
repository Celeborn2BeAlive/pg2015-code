#include "VoxelBuffer.hpp"

#include <bonez/opengl/opengl.hpp>

namespace BnZ {

int VoxelBuffer::s_bitmask[] = {
    1 << 0,
    1 << 1,
    1 << 2,
    1 << 3,
    1 << 4,
    1 << 5,
    1 << 6,
    1 << 7,
    1 << 8,
    1 << 9,
    1 << 10,
    1 << 11,
    1 << 12,
    1 << 13,
    1 << 14,
    1 << 15,
    1 << 16,
    1 << 17,
    1 << 18,
    1 << 19,
    1 << 20,
    1 << 21,
    1 << 22,
    1 << 23,
    1 << 24,
    1 << 25,
    1 << 26,
    1 << 27,
    1 << 28,
    1 << 29,
    1 << 30,
    1 << 31
};

void VoxelBuffer::init(const GLVoxelFramebuffer& framebuffer) {
    m_width = framebuffer.width();
    m_height = framebuffer.height();
    m_depth = framebuffer.depth();
    m_numRenderTarget = framebuffer.getTextureCount();

    m_data.resize(m_width * m_height * m_numRenderTarget);

    framebuffer.bind(GL_READ_FRAMEBUFFER);

    // Read all textures
    for(auto i = 0u; i < m_numRenderTarget; ++i) {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        glReadPixels(0, 0, m_width, m_height, GL_RGBA_INTEGER, GL_INT, m_data.data() + m_width * m_height * i);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

bool VoxelBuffer::operator()(uint32_t x, uint32_t y, uint32_t z) {
    auto colorBuffer = z / 128;
    auto absoluteBitPosition = z % 128;
    auto ivecPosition = absoluteBitPosition / 32;
    auto bitPosition = absoluteBitPosition % 32;

    return ((m_data[x + y * m_width + colorBuffer * m_width * m_height][ivecPosition] & s_bitmask[bitPosition]) != 0);
}

}
