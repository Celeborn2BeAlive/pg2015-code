#include "GLCubicalComplexRenderer.hpp"

namespace BnZ {

void GLCubicalComplexRenderer::drawCubicalComplex(const glm::mat4& gridToWorld,
                                    const GLCubicalComplex& cubicalComplex) {
    glm::mat4 MVP = m_ProjectionMatrix * m_ViewMatrix * gridToWorld;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, cubicalComplex.getTexture().glId());

    m_RenderCubicalComplexFacesPass.m_Program.use();

    glUniformMatrix4fv(m_RenderCubicalComplexFacesPass.m_uMVPLocation, 1,
                       GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(m_RenderCubicalComplexFacesPass.m_uCubicalComplexLocation, 0);

    cubicalComplex.render();

    m_RenderCubicalComplexEdgesPass.m_Program.use();

    glUniformMatrix4fv(m_RenderCubicalComplexEdgesPass.m_uMVPLocation, 1,
                       GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(m_RenderCubicalComplexEdgesPass.m_uCubicalComplexLocation, 0);

    glLineWidth(4.f);

    cubicalComplex.render();

    m_RenderCubicalComplexPointsPass.m_Program.use();

    glUniformMatrix4fv(m_RenderCubicalComplexPointsPass.m_uMVPLocation, 1,
                       GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(m_RenderCubicalComplexPointsPass.m_uCubicalComplexLocation, 0);

    cubicalComplex.render();
}

GLCubicalComplexRenderer::GLCubicalComplexRenderer(const GLShaderManager& shaderManager):
    m_RenderCubicalComplexFacesPass(shaderManager),
    m_RenderCubicalComplexEdgesPass(shaderManager),
    m_RenderCubicalComplexPointsPass(shaderManager) {
}

GLCubicalComplexRenderer::RenderCubicalComplexFacesPass::RenderCubicalComplexFacesPass(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/CComplex.vs", "voxskel/CComplexFaces.gs", "voxskel/CComplex.fs" })) {

    m_Program.use();

    m_uMVPLocation = m_Program.getUniformLocation("MVP");
    m_uMVLocation = m_Program.getUniformLocation("MV");
    m_uCubicalComplexLocation = m_Program.getUniformLocation("uCubicalComplex");
}

GLCubicalComplexRenderer::RenderCubicalComplexEdgesPass::RenderCubicalComplexEdgesPass(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/CComplex.vs", "voxskel/CComplexEdges.gs", "voxskel/CComplex.fs" })) {

    m_Program.use();

    m_uMVPLocation = m_Program.getUniformLocation("MVP");
    m_uMVLocation = m_Program.getUniformLocation("MV");
    m_uCubicalComplexLocation = m_Program.getUniformLocation("uCubicalComplex");
}

GLCubicalComplexRenderer::RenderCubicalComplexPointsPass::RenderCubicalComplexPointsPass(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/CComplex.vs", "voxskel/CComplexPoints.gs", "voxskel/CComplex.fs" })) {

    m_Program.use();

    m_uMVPLocation = m_Program.getUniformLocation("MVP");
    m_uMVLocation = m_Program.getUniformLocation("MV");
    m_uCubicalComplexLocation = m_Program.getUniformLocation("uCubicalComplex");
}

}
