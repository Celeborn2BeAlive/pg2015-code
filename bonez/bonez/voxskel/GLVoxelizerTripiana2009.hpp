#pragma once

#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/maths/BBox.hpp>

#include "GLVoxelFramebuffer.hpp"

namespace BnZ {

// Allow to voxelize a 3D Model using the method from Carlos Tripiana Montes
// Ref: http://voxskel.files.wordpress.com/2013/06/gpu-voxelization-c-tripiana-montes.pdf
class GLVoxelizerTripiana2009 {
public:
    static const GLuint VERTEX_ATTRIB_POSITION = 0u;

    GLVoxelizerTripiana2009(const GLShaderManager& shaderManager);

    ~GLVoxelizerTripiana2009();

    // Initialize required states of OpenGL
    // After calling this method, render the model with glDraw* functions.
    // The vertex attribute corresponding to the 3D positions must have
    // the index VERTEX_ATTRIB_POSITION in the VAO.
    //
    // \param bbox The bounding box of the model to voxelize
    // \param framebuffer The framebuffer in which to store the voxelization
    void initGLState(uint32_t resolution, BBox3f bbox,
                     GLVoxelFramebuffer& framebuffer, Mat4f& gridToWorldMatrix);

    // Restore affected OpenGL states to their default values
    // Call this method after performing the rendering of the model
    void restoreGLState();

private:
    GLProgram m_Program;

    BNZ_GLUNIFORM(m_Program, Mat4f, MVP);
    BNZ_GLUNIFORM(m_Program, Vec2f, halfPixelSize);
    BNZ_GLUNIFORM(m_Program, int, numVoxels);
    BNZ_GLUNIFORM(m_Program, Vec3f, origBBox);
    BNZ_GLUNIFORM(m_Program, int, numRenderTargets);
    BNZ_GLUNIFORM(m_Program, float, voxelSize);
};

}
