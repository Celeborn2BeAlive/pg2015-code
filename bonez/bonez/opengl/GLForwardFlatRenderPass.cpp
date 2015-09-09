#include "GLForwardFlatRenderPass.hpp"

namespace BnZ {

GLFlatForwardRenderPass::GLFlatForwardRenderPass(const GLShaderManager &shaderManager):
    m_Program(shaderManager.buildProgram({"forwardPass.vs", "flatForwardPass.fs"})) {
}

void GLFlatForwardRenderPass::render(const Mat4f& mvpMatrix, const GLScene& scene) {
    m_Program.use();

    uMVPMatrix.set(mvpMatrix);

    glEnable(GL_DEPTH_TEST);

    scene.render(uMaterial);
}

}
