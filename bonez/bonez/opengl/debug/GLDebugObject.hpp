#pragma once

#include <bonez/types.hpp>
#include <bonez/opengl/utils/GLBuffer.hpp>
#include <bonez/opengl/utils/GLVertexArray.hpp>

namespace BnZ {

struct GLDebugObject {
    struct Vertex {
        Vec3f position;
        Vec3f normal;

        Vertex(Vec3f p, Vec3f n): position(p), normal(n) {
        }
    };

    GLDebugObject();

    void setInstanceBuffer(GLuint instanceBuffer);

    void draw();

    void draw(uint32_t nbInstances);

    friend void buildSphere(GLsizei discLat, GLsizei discLong, GLDebugObject& model);

    friend void buildDisk(GLsizei discBase, GLDebugObject& model);

    friend void buildCircle(GLsizei discBase, GLDebugObject& model);

    friend void buildCone(GLsizei discLat, GLsizei discHeight, GLDebugObject& model);

    friend void buildCylinder(GLsizei discLat, GLsizei discHeight, GLDebugObject& model);

private:
    GLBuffer<Vertex> VBO;
    GLBuffer<uint16_t> IBO;
    GLVertexArray VAO;
    GLenum primType;
};

struct GLDebugObjectInstance {
    Vec3f color;
    Mat4f mvpMatrix;
    Mat4f mvMatrix;
    Vec4u objectID;

    GLDebugObjectInstance(const Mat4f& modelMatrix, const Vec3f& color,
                   const Vec4u& objectID):
        color(color), mvpMatrix(modelMatrix), mvMatrix(modelMatrix), objectID(objectID) {
    }
};

}
