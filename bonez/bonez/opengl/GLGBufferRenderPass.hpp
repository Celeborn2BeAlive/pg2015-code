#pragma once

#include "utils/GLutils.hpp"
#include "GLGBuffer.hpp"
#include "GLScene.hpp"
#include "GLShaderManager.hpp"

namespace BnZ {

class GLGBufferRenderPass {
    GLProgram m_Program;

    GLMaterialUniforms m_MaterialUniforms { m_Program };
    BNZ_GLUNIFORM(m_Program, Mat4f, uMVPMatrix);
    BNZ_GLUNIFORM(m_Program, Mat4f, uMVMatrix);
    BNZ_GLUNIFORM(m_Program, float, uZFar);

public:
    GLGBufferRenderPass(const GLShaderManager& shaderManager);

    void render(const Mat4f& projMatrix, const Mat4f& viewMatrix, float zFar,
                const GLScene& scene, GLGBuffer& gbuffer);
};

}
