#pragma once

#include "GLShaderManager.hpp"
#include "GLScene.hpp"

namespace BnZ {

class GLFlatForwardRenderPass {
public:
    GLFlatForwardRenderPass(const GLShaderManager& shaderManager);

    void render(const Mat4f& mvpMatrix, const GLScene& scene);
private:
    GLProgram m_Program;
    BNZ_GLUNIFORM(m_Program, Mat4f, uMVPMatrix);
    GLMaterialUniforms uMaterial { m_Program };
};

}
