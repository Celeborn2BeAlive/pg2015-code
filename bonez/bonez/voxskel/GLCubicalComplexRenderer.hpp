#pragma once

#include <bonez/maths/glm.hpp>
#include <bonez/opengl/GLShaderManager.hpp>

#include "GLCubicalComplex.hpp"

namespace BnZ {

class GLCubicalComplexRenderer {
public:
    GLCubicalComplexRenderer(const GLShaderManager& shaderManager);

    void setProjectionMatrix(glm::mat4 projectionMatrix) {
        m_ProjectionMatrix = projectionMatrix;
    }

    void setViewMatrix(glm::mat4 viewMatrix) {
        m_ViewMatrix = viewMatrix;
    }

    void drawCubicalComplex(const glm::mat4& gridToWorld,
                            const GLCubicalComplex& cubicalComplex);

private:
    glm::mat4 m_ProjectionMatrix;
    glm::mat4 m_ViewMatrix;

    struct RenderCubicalComplexFacesPass {
        GLProgram m_Program;

        GLint m_uMVLocation;
        GLint m_uMVPLocation;
        GLint m_uCubicalComplexLocation;

        RenderCubicalComplexFacesPass(const GLShaderManager& shaderManager);
    };
    RenderCubicalComplexFacesPass m_RenderCubicalComplexFacesPass;

    struct RenderCubicalComplexEdgesPass {
        GLProgram m_Program;

        GLint m_uMVLocation;
        GLint m_uMVPLocation;
        GLint m_uCubicalComplexLocation;

        RenderCubicalComplexEdgesPass(const GLShaderManager& shaderManager);
    };
    RenderCubicalComplexEdgesPass m_RenderCubicalComplexEdgesPass;

    struct RenderCubicalComplexPointsPass {
        GLProgram m_Program;

        GLint m_uMVLocation;
        GLint m_uMVPLocation;
        GLint m_uCubicalComplexLocation;

        RenderCubicalComplexPointsPass(const GLShaderManager& shaderManager);
    };
    RenderCubicalComplexPointsPass m_RenderCubicalComplexPointsPass;
};

}
