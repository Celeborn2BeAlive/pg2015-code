#include "GLDebugObject.hpp"

#include <bonez/maths/constants.hpp>
#include <bonez/opengl/utils/GLutils.hpp>

namespace BnZ {

GLDebugObject::GLDebugObject() {
    VAO.enableVertexAttrib(0);
    VAO.enableVertexAttrib(1);
    VAO.vertexAttribOffset(VBO.glId(), 0, 3, GL_FLOAT, GL_FALSE,
                           sizeof(Vertex), BNZ_OFFSETOF(Vertex, position));
    VAO.vertexAttribOffset(VBO.glId(), 1, 3, GL_FLOAT, GL_FALSE,
                           sizeof(Vertex), BNZ_OFFSETOF(Vertex, normal));
}

void GLDebugObject::setInstanceBuffer(GLuint instanceBuffer) {
    VAO.bind();

    VAO.enableVertexAttrib(2);
    VAO.vertexAttribOffset(instanceBuffer, 2, 3, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, color));
    glVertexAttribDivisor(2, 1);

    VAO.enableVertexAttrib(3);
    VAO.vertexAttribOffset(instanceBuffer, 3, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvpMatrix) + 0 * sizeof(Vec4f));
    glVertexAttribDivisor(3, 1);

    VAO.enableVertexAttrib(4);
    VAO.vertexAttribOffset(instanceBuffer, 4, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvpMatrix) + 1 * sizeof(Vec4f));
    glVertexAttribDivisor(4, 1);

    VAO.enableVertexAttrib(5);
    VAO.vertexAttribOffset(instanceBuffer, 5, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvpMatrix) + 2 * sizeof(Vec4f));
    glVertexAttribDivisor(5, 1);

    VAO.enableVertexAttrib(6);
    VAO.vertexAttribOffset(instanceBuffer, 6, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvpMatrix) + 3 * sizeof(Vec4f));
    glVertexAttribDivisor(6, 1);

    VAO.enableVertexAttrib(7);
    VAO.vertexAttribOffset(instanceBuffer, 7, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvMatrix) + 0 * sizeof(Vec4f));
    glVertexAttribDivisor(7, 1);

    VAO.enableVertexAttrib(8);
    VAO.vertexAttribOffset(instanceBuffer, 8, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvMatrix) + 1 * sizeof(Vec4f));
    glVertexAttribDivisor(8, 1);

    VAO.enableVertexAttrib(9);
    VAO.vertexAttribOffset(instanceBuffer, 9, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvMatrix) + 2 * sizeof(Vec4f));
    glVertexAttribDivisor(9, 1);

    VAO.enableVertexAttrib(10);
    VAO.vertexAttribOffset(instanceBuffer, 10, 4, GL_FLOAT, GL_FALSE,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, mvMatrix) + 3 * sizeof(Vec4f));
    glVertexAttribDivisor(10, 1);

    VAO.enableVertexAttrib(11);
    VAO.vertexAttribIOffset(instanceBuffer, 11, 4, GL_UNSIGNED_INT,
                           sizeof(GLDebugObjectInstance), BNZ_OFFSETOF(GLDebugObjectInstance, objectID));
    glVertexAttribDivisor(11, 1);
}

void GLDebugObject::draw() {
    VAO.bind();
    IBO.bind(GL_ELEMENT_ARRAY_BUFFER);
    glDrawElements(primType, IBO.size(), GL_UNSIGNED_SHORT, 0);
}

void GLDebugObject::draw(uint32_t nbInstances) {
    VAO.bind();
    IBO.bind(GL_ELEMENT_ARRAY_BUFFER);
    glDrawElementsInstanced(primType, IBO.size(), GL_UNSIGNED_SHORT, 0, nbInstances);
}

void buildSphere(GLsizei discLat, GLsizei discLong, GLDebugObject& model) {
    GLfloat rcpLat = 1.f / discLat, rcpLong = 1.f / discLong;
    GLfloat dPhi = pi<float>() * 2.f * rcpLat, dTheta = pi<float>() * rcpLong;

    std::vector<GLDebugObject::Vertex> vertices;

    float pi_over_two = pi<float>() / 2.f;

    std::cerr << discLong << std::endl;
    std::cerr << discLat << std::endl;

    // Construit l'ensemble des vertex
    for(GLsizei j = 0; j <= discLong; ++j) {
        GLfloat cosTheta = cos(-pi_over_two + j * dTheta);
        GLfloat sinTheta = sin(-pi_over_two + j * dTheta);

        for(GLsizei i = 0; i <= discLat; ++i) {
            Vec3f coords;

            coords.x = sin(i * dPhi) * cosTheta;
            coords.y = sinTheta;
            coords.z = cos(i * dPhi) * cosTheta;

            vertices.emplace_back(coords, coords);
        }
    }

    std::vector<uint16_t> indices;

    for(GLsizei j = 0; j < discLong; ++j) {
        GLsizei offset = j * (discLat + 1);
        for(GLsizei i = 0; i < discLat; ++i) {
            indices.push_back(offset + i);
            indices.push_back(offset + (i + 1));
            indices.push_back(offset + discLat + 1 + (i + 1));

            indices.push_back(offset + i);
            indices.push_back(offset + discLat + 1 + (i + 1));
            indices.push_back(offset + i + discLat + 1);
        }
    }

    model.VBO.setData(vertices, GL_STATIC_DRAW);
    model.IBO.setData(indices, GL_STATIC_DRAW);
    model.primType = GL_TRIANGLES;
}

void buildDisk(GLsizei discBase, GLDebugObject& model) {
    GLfloat rcpBase = 1.f / discBase;
    GLfloat dPhi = pi<float>() * 2.f * rcpBase;

    std::vector<GLDebugObject::Vertex> vertices;

    // Construit l'ensemble des vertex

    Vec3f normal(0, 1, 0);

    // Origin
    vertices.emplace_back(Vec3f(0), normal);

    for(GLsizei i = 0; i <= discBase; ++i) {
        vertices.emplace_back(Vec3f(sin(i * dPhi), 0, cos(i * dPhi)), normal);
    }

    std::vector<uint16_t> indices;

    for(GLsizei i = 0; i < discBase; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }

    model.VBO.setData(vertices, GL_STATIC_DRAW);
    model.IBO.setData(indices, GL_STATIC_DRAW);
    model.primType = GL_TRIANGLES;
}

void buildCircle(GLsizei discBase, GLDebugObject& model) {
    GLfloat rcpBase = 1.f / discBase;
    GLfloat dPhi = pi<float>() * 2.f * rcpBase;

    std::vector<GLDebugObject::Vertex> vertices;

    // Construit l'ensemble des vertex

    Vec3f normal(0, 1, 0);

    for(GLsizei i = 0; i <= discBase; ++i) {
        vertices.emplace_back(Vec3f(sin(i * dPhi), 0, cos(i * dPhi)),
                                         normal);
    }

    std::vector<uint16_t> indices;

    for(GLsizei i = 0; i < discBase; ++i) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    model.VBO.setData(vertices, GL_STATIC_DRAW);
    model.IBO.setData(indices, GL_STATIC_DRAW);
    model.primType = GL_LINES;
}

void buildCone(GLsizei discLat, GLsizei discHeight, GLDebugObject& model) {
    GLfloat rcpLat = 1.f / discLat, rcpH = 1.f / discHeight;
    GLfloat dPhi = pi<float>() * 2.f * rcpLat, dH = rcpH;

    std::vector<GLDebugObject::Vertex> vertices;

    // Construit l'ensemble des vertex
    for(GLsizei j = 0; j <= discHeight; ++j) {
        for(GLsizei i = 0; i < discLat; ++i) {
            Vec3f position, normal;

            position.x = (1.f - j * dH) * sin(i * dPhi);
            position.y = j * dH;
            position.z = (1.f - j * dH) * cos(i * dPhi);

            normal.x = sin(i * dPhi);
            normal.y = 1.f;
            normal.z = cos(i * dPhi);

            vertices.emplace_back(position, normalize(normal));
        }
    }

    std::vector<uint16_t> indices;

    // Construit les vertex finaux en regroupant les données en triangles:
    // Pour une longitude donnée, les deux triangles formant une face sont de la forme:
    // (i, i + 1, i + discLat + 1), (i, i + discLat + 1, i + discLat)
    // avec i sur la bande correspondant à la longitude
    for(GLsizei j = 0; j < discHeight; ++j) {
        GLsizei offset = j * discLat;
        for(GLsizei i = 0; i < discLat; ++i) {
            indices.push_back(offset + i);
            indices.push_back(offset + (i + 1) % discLat);
            indices.push_back(offset + discLat + (i + 1) % discLat);

            indices.push_back(offset + i);
            indices.push_back(offset + discLat + (i + 1) % discLat);
            indices.push_back(offset + i + discLat);
        }
    }

    model.VBO.setData(vertices, GL_STATIC_DRAW);
    model.IBO.setData(indices, GL_STATIC_DRAW);
    model.primType = GL_TRIANGLES;
}

void buildCylinder(GLsizei discLat, GLsizei discHeight, GLDebugObject& model) {
    GLfloat rcpLat = 1.f / discLat, rcpH = 1.f / discHeight;
    GLfloat dPhi = pi<float>() * 2.f * rcpLat, dH = rcpH;

    std::vector<GLDebugObject::Vertex> vertices;

    // Construit l'ensemble des vertex
    for(GLsizei j = 0; j <= discHeight; ++j) {
        for(GLsizei i = 0; i < discLat; ++i) {
            Vec3f position, normal;

            normal.x = sin(i * dPhi);
            normal.y = 0;
            normal.z = cos(i * dPhi);

            position = normal;
            position.y = j * dH;

            vertices.emplace_back(position, normal);
        }
    }

    std::vector<uint16_t> indices;

    for(GLsizei j = 0; j < discHeight; ++j) {
        GLsizei offset = j * discLat;
        for(GLsizei i = 0; i < discLat; ++i) {
            indices.push_back(offset + i);
            indices.push_back(offset + (i + 1) % discLat);
            indices.push_back(offset + discLat + (i + 1) % discLat);

            indices.push_back(offset + i);
            indices.push_back(offset + discLat + (i + 1) % discLat);
            indices.push_back(offset + i + discLat);
        }
    }

    model.VBO.setData(vertices, GL_STATIC_DRAW);
    model.IBO.setData(indices, GL_STATIC_DRAW);
    model.primType = GL_TRIANGLES;
}


}
