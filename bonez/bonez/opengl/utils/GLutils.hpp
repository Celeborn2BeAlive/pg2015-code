#pragma once

#include <bonez/types.hpp>
#include "GLShader.hpp"
#include "GLProgram.hpp"
#include "GLFramebuffer.hpp"
#include "GLCubeFramebuffer.hpp"
#include "GLQuery.hpp"
#include "GLBuffer.hpp"
#include "GLState.hpp"
#include "GLUniform.hpp"
#include "GLVertexArray.hpp"
#include "GLTexture.hpp"
#include "GLBindlessTexture.hpp"
#include "glGet.hpp"

namespace BnZ {

// A full screen triangle
struct GLScreenTriangle {
    GLBufferStorage<float> vbo;
    GLVertexArray vao;

    GLScreenTriangle(GLuint attribLocation = 0):
        vbo({ -1, -1, 3, -1, -1, 3 }) {
        vao.enableVertexAttrib(attribLocation);
        vao.vertexAttribOffset(vbo.glId(), attribLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }

    void render() const {
        vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
};

inline GLintptr bufferOffset(size_t offset) {
    return GLintptr(offset);
}

#define BNZ_OFFSETOF(TYPE, MEMBER) BnZ::bufferOffset(offsetof(TYPE, MEMBER))

}
