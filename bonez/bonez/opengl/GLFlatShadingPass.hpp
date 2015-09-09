#pragma once

#include <bonez/viewer/GUI.hpp>

#include "utils/GLutils.hpp"
#include "GLGBuffer.hpp"
#include "GLScene.hpp"
#include "GLShaderManager.hpp"

namespace BnZ {

class GLFlatShadingPass {
    GLProgram m_Program;

    GLGBufferUniform uGBuffer { m_Program };
    BNZ_GLUNIFORM(m_Program, Mat4f, uRcpProjMatrix);
    BNZ_GLUNIFORM(m_Program, bool, uDoWhiteRendering);
    BNZ_GLUNIFORM(m_Program, bool, uDoLightingFromCamera);
    BNZ_GLUNIFORM(m_Program, Mat4f, uViewMatrix);

    bool m_bDoWhiteRendering = false;
    bool m_bDoLightingFromCamera = true;

public:
    GLFlatShadingPass(const GLShaderManager& shaderManager);

    void render(const GLGBuffer& gBuffer,
                const Mat4f& rcpProjMatrix,
                const Mat4f& viewMatrix,
                const GLScreenTriangle& triangle);

    void exposeIO(GUI& gui);
};

}
