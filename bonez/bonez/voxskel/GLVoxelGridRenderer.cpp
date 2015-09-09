#include "GLVoxelGridRenderer.hpp"

namespace BnZ {

void GLVoxelGrid::init(const VoxelGrid &voxelGrid) {
    int size_x = voxelGrid.width();
    int size_y = voxelGrid.height();
    int size_z = voxelGrid.depth();

    std::vector<Vertex> vertices;

    glBindTexture(GL_TEXTURE_3D, m_TextureObject);

    std::vector<int32_t> tmp(size_x * size_y * size_z);
    for(auto i = 0u; i < tmp.size(); ++i) {
        tmp[i] = voxelGrid[i];
    }

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, size_x, size_y, size_z, 0,
                 GL_RED_INTEGER, GL_INT, tmp.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_3D, 0);

    for(int z = 0; z < size_z; ++z) {
        for(int y = 0; y < size_y; ++y) {
            for(int x = 0; x < size_x; ++x) {
                vertices.emplace_back(glm::ivec3(x, y, z));
            }
        }
    }

    m_nVertexCount = vertices.size();

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glBindVertexArray(m_VAO);

    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 3, GL_INT, sizeof(Vertex), (const GLvoid*) 0);
}

void GLVoxelGridRenderer::render(const glm::mat4& gridToWorld,
                                 const GLVoxelGrid& voxelGrid,
                                 GLuint colorTexture,
                                 bool useNeighborColors) {
    glm::mat4 MVP = m_ProjectionMatrix * m_ViewMatrix * gridToWorld;

    glEnable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, voxelGrid.getTextureObject());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, colorTexture);

    m_RenderFacesPass.m_Program.use();

    glUniformMatrix4fv(m_RenderFacesPass.m_uMVPLocation, 1,
                       GL_FALSE, glm::value_ptr(MVP));
    glUniform1i(m_RenderFacesPass.m_uCubicalComplexLocation, 0);
    m_RenderFacesPass.uColorSampler.set(1);

    m_RenderFacesPass.uColor.set(Vec3f(1.f));

    m_RenderFacesPass.uUseNeighborColors.set(useNeighborColors);

    voxelGrid.render();

    // Restore GL states
    glUseProgram(0u);
    glBindTexture(GL_TEXTURE_3D, 0u);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0u);
}

GLVoxelGridRenderer::GLVoxelGridRenderer(const GLShaderManager& shaderManager):
    m_RenderFacesPass(shaderManager)
{
}

GLVoxelGridRenderer::RenderFacesPass::RenderFacesPass(const GLShaderManager& shaderManager):
    m_Program(shaderManager.buildProgram({ "voxskel/CComplex.vs", "voxskel/display_voxel_face.gs", "voxskel/display_voxel.fs" })) {

    m_Program.use();

    m_uMVPLocation = m_Program.getUniformLocation("MVP");
    m_uMVLocation = m_Program.getUniformLocation("MV");
    m_uCubicalComplexLocation = m_Program.getUniformLocation("uCubicalComplex");
}

}
