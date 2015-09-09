#include "GLCubicalComplexFaceListRenderer.hpp"

namespace BnZ {

void GLCubicalComplexFaceList::init(const Vertex* vertices, size_t count) {
    m_VBO.setData(count, vertices, GL_STATIC_DRAW);

    m_VAO.enableVertexAttrib(FACE_ATTR_LOCATION);
    m_VAO.enableVertexAttrib(FACECOLORXY_ATTR_LOCATION);
    m_VAO.enableVertexAttrib(FACECOLORYZ_LOCATION);
    m_VAO.enableVertexAttrib(FACECOLORXZ_ATTR_LOCATION);

    m_VAO.vertexAttribIOffset(m_VBO.glId(), FACE_ATTR_LOCATION, 4, GL_INT, sizeof(Vertex), BNZ_OFFSETOF(Vertex, m_Face));
    m_VAO.vertexAttribOffset(m_VBO.glId(), FACECOLORXY_ATTR_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BNZ_OFFSETOF(Vertex, m_FaceColorXY));
    m_VAO.vertexAttribOffset(m_VBO.glId(), FACECOLORYZ_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BNZ_OFFSETOF(Vertex, m_FaceColorYZ));
    m_VAO.vertexAttribOffset(m_VBO.glId(), FACECOLORXZ_ATTR_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BNZ_OFFSETOF(Vertex, m_FaceColorXZ));
}

void GLCubicalComplexFaceList::render() const {
    m_VAO.bind();
    glDrawArrays(GL_POINTS, 0, m_VBO.size());
}

GLCubicalComplexFaceListRenderer::GLCubicalComplexFaceListRenderer(
        const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/CComplexFaceList.vs",
                                           "voxskel/CComplexFaceList.gs",
                                           "voxskel/CComplexFaceList.fs" })) {
}

void GLCubicalComplexFaceListRenderer::render(
        const GLCubicalComplexFaceList& faceList) {
    glEnable(GL_DEPTH_TEST);

    m_Program.use();

    uMVPMatrix.set(m_MVPMatrix);

    faceList.render();

    glUseProgram(0u);
    glDisable(GL_DEPTH_TEST);
}

}
