#include "GLVoxelFramebuffer.hpp"
#include "VoxelBuffer.hpp"

namespace BnZ {

GLVoxelFramebuffer::GLVoxelFramebuffer(): m_FBO(0), m_renderedTexture(nullptr){
    //Init the Framebuffer Object

}

bool GLVoxelFramebuffer::init(int width, int height, int depth, int numTextures){
    if(m_renderedTexture) {
        glDeleteTextures(m_numTextures, m_renderedTexture.get());
    }

    if(m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
    }

    glGenFramebuffers(1, &m_FBO);

    m_renderedTexture = makeUniqueArray<GLuint>(numTextures);

    //bind the Framebuffer Object
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    m_height = height;
    m_width = width;
    m_depth = depth;
    m_numTextures = numTextures;

    //Init & bind the rendered texture
    glGenTextures(numTextures, m_renderedTexture.get());

    for(int i = 0; i < numTextures; ++i){
        glBindTexture(GL_TEXTURE_2D, m_renderedTexture[i]);

        // Give an empty image to OpenGL
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, width, height, 0, GL_RGBA_INTEGER, GL_INT, 0);

        // Poor filtering. Needed !
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Set "renderedTexture" as our colour attachement #0
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_renderedTexture[i], 0);
    }

    // Check that our framebuffer is ok
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        return false;
    }

    return true;
}

GLVoxelFramebuffer::~GLVoxelFramebuffer(){
    if(m_renderedTexture) {
        glDeleteTextures(m_numTextures, m_renderedTexture.get());
    }
    glDeleteFramebuffers(1,&m_FBO);
}

VoxelGrid getVoxelGrid(const GLVoxelFramebuffer& framebuffer, bool getComplementary) {
    VoxelGrid voxelGrid(framebuffer.width(), framebuffer.height(), framebuffer.depth(), 0);

    // Will be used to store the data of a ColorBuffer
    VoxelBuffer voxelBuffer;

    //Store the data in the VoxelBuffer
    voxelBuffer.init(framebuffer);

    auto d = framebuffer.depth();
    // Convert data in voxel position
    auto processVoxel = [d, &voxelGrid](int x, int y, int z) {
          voxelGrid(x, y, z) = 1;
    };

    voxelBuffer.processVoxels(processVoxel, getComplementary);

    return voxelGrid;
}

CubicalComplex3D getCubicalComplexFromVoxelFramebuffer(const GLVoxelFramebuffer& framebuffer, bool getComplementary) {
    CubicalComplex3D cubicalComplex(framebuffer.width(), framebuffer.height(), framebuffer.depth());

    /// Will be used to store the data of a ColorBuffer
    VoxelBuffer voxelBuffer;

    //Store the data in the VoxelBuffer
    voxelBuffer.init(framebuffer);

    auto w = framebuffer.width();
    auto h = framebuffer.height();
    auto d = framebuffer.depth();

    auto processVoxel = [w, h, d, &cubicalComplex, &voxelBuffer](int x, int y, int z) {
        cubicalComplex(x, y, z).fill();

        auto value = voxelBuffer(x, y, z);

        if (x == w - 1 || value != voxelBuffer(x + 1, y, z)) cubicalComplex(x + 1, y, z).add(CC3DFaceBits::POINT | CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE | CC3DFaceBits::YZFACE);

        if (y == h - 1 || value != voxelBuffer(x, y + 1, z)) cubicalComplex(x, y + 1, z).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE | CC3DFaceBits::ZEDGE | CC3DFaceBits::XZFACE);

        if (z == d - 1 || value != voxelBuffer(x, y, z + 1)) cubicalComplex(x, y, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE | CC3DFaceBits::XYFACE);

        if (x == w - 1 || y == h - 1 || value != voxelBuffer(x + 1, y + 1, z)) cubicalComplex(x + 1, y + 1, z).add(CC3DFaceBits::POINT | CC3DFaceBits::ZEDGE);

        if (x == w - 1 || z == d - 1 || value != voxelBuffer(x + 1, y, z + 1)) cubicalComplex(x + 1, y, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::YEDGE);

        if (y == h - 1 || z == d - 1 || value != voxelBuffer(x, y + 1, z + 1)) cubicalComplex(x, y + 1, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE);

        if (x == w - 1 || y == h - 1 || z == d - 1 || value != voxelBuffer(x + 1, y + 1, z + 1)) cubicalComplex(x + 1, y + 1, z + 1).add(CC3DFaceBits::POINT);
    };

    voxelBuffer.processVoxels(processVoxel, getComplementary);

    return cubicalComplex;
}

}
