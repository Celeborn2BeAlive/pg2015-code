#pragma once

#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/utils/GLutils.hpp>

namespace BnZ {

class GLCubeMappingMVPMatrixBufferComputePass {
public:
    GLCubeMappingMVPMatrixBufferComputePass(const GLShaderManager& shaderManager):
        m_Program(shaderManager.buildProgram(
            { "cube_mapping/buildCubeMappingMVPMatrixBuffer.cs" }
        )) {
    }

    void compute(const Mat4f* faceProjMatrix,
                 uint32_t mvCount,
                 const GLBufferStorage<Mat4f>& mvMatrixBuffer,
                 const GLBufferStorage<Mat4f>& mvpMatrixBuffer
                 ) {
        m_Program.use();

        uFaceProjMatrix.set(6, faceProjMatrix);
        //uMVMatrixBuffer.set(mvMatrixBuffer);
        uMVCount.set(mvCount);
        //uMVPMatrixBuffer.set(mvpMatrixBuffer);

        mvMatrixBuffer.bindBase(GL_SHADER_STORAGE_BUFFER, 0);
        mvpMatrixBuffer.bindBase(GL_SHADER_STORAGE_BUFFER, 1);

        uint32_t localSize = 1024;
        glDispatchCompute(1 + mvCount / localSize, 1, 1);

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

private:
    GLProgram m_Program;

    BNZ_GLUNIFORM(m_Program, Mat4f[], uFaceProjMatrix);
    //BNZ_GLUNIFORM(m_Program, GLBufferAddress<Mat4f>, uMVMatrixBuffer);
    BNZ_GLUNIFORM(m_Program, GLuint, uMVCount);
    //BNZ_GLUNIFORM(m_Program, GLBufferAddress<Mat4f>, uMVPMatrixBuffer);
};

}
