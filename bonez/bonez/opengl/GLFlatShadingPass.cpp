#include "GLFlatShadingPass.hpp"

namespace BnZ {

GLFlatShadingPass::GLFlatShadingPass(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "deferredShadingPass.vs", "flatDeferredShadingPass.fs"})) {
}

void GLFlatShadingPass::render(const GLGBuffer& gBuffer,
                               const Mat4f& rcpProjMatrix,
                               const Mat4f& viewMatrix,
                               const GLScreenTriangle& triangle) {
    m_Program.use();

    uGBuffer.set(gBuffer, 0, 1, 2);
    uRcpProjMatrix.set(rcpProjMatrix);
    uViewMatrix.set(viewMatrix);
    uDoLightingFromCamera.set(m_bDoLightingFromCamera);
    uDoWhiteRendering.set(m_bDoWhiteRendering);

    triangle.render();
}

void GLFlatShadingPass::exposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_bDoLightingFromCamera));
    gui.addVarRW(BNZ_GUI_VAR(m_bDoWhiteRendering));
}

}
