#pragma once

#include <bonez/opengl/opengl.hpp>
#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/maths/glm.hpp>

#include "CubicalComplex3D.hpp"

namespace BnZ {

class GLCubicalComplex {
public:
    struct CCVertex{
        glm::ivec3 m_position;

        CCVertex(glm::ivec3 position):m_position(position) {}
    };

    const GLTexture3D& getTexture() const {
        return m_CCTexture;
    }

    void init(const CubicalComplex3D& cubicalComplex);

    void render() const {
        m_VAO.bind();
        glDrawArrays(GL_POINTS, 0, m_VBO.size());
        glBindVertexArray(0);
    }
private:
    GLBuffer<CCVertex> m_VBO;
    GLVertexArray m_VAO;
    GLTexture3D m_CCTexture;
};

}
