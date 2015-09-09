#include "GLCubicalComplex.hpp"
#include <iostream>

namespace BnZ {

void GLCubicalComplex::init(const CubicalComplex3D &cubicalComplex) {
    int size_x = cubicalComplex.width();
    int size_y = cubicalComplex.height();
    int size_z = cubicalComplex.depth();

    std::vector<int32_t> tmp(size_x * size_y * size_z);

    for(auto i = 0u; i < tmp.size(); ++i) {
        tmp[i] = cubicalComplex[i];
    }

    m_CCTexture.setMagFilter(GL_NEAREST);
    m_CCTexture.setMinFilter(GL_NEAREST);
    m_CCTexture.setImage(0, GL_R8I, size_x, size_y, size_z, 0,
                         GL_RED_INTEGER, GL_INT, tmp.data());

    std::vector<CCVertex> CCVertices;

    for(int z = 0; z < size_z; ++z) {
        for(int y = 0; y < size_y; ++y) {
            for(int x = 0; x < size_x; ++x) {
                glm::ivec3 position(x, y, z);
                CCVertices.emplace_back(CCVertex(position));
            }
        }
    }

    m_VBO.setData(CCVertices, GL_STATIC_DRAW);
    m_VAO.enableVertexAttrib(0);
    m_VAO.vertexAttribIOffset(m_VBO.glId(), 0, 3, GL_INT, sizeof(CCVertex), 0);
}

}
