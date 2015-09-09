#pragma once

#include "GLObject.hpp"

namespace BnZ {

class GLVertexArray: GLVertexArrayObject {
public:
    using GLVertexArrayObject::glId;

    GLVertexArray() = default;

    GLVertexArray(GLVertexArray&& rvalue) : GLVertexArrayObject(std::move(rvalue)) {
    }

    GLVertexArray& operator =(GLVertexArray&& rvalue) {
        GLVertexArrayObject::operator=(std::move(rvalue));
        return *this;
    }

    void bind() const {
        glBindVertexArray(glId());
    }

    void vertexAttribOffset(GLuint buffer, GLuint index, GLuint size, GLenum type, GLboolean normalized,
                            GLsizei stride, GLintptr offset) {
        glVertexArrayVertexAttribOffsetEXT(glId(), buffer, index, size, type, normalized,
                                           stride, offset);
    }

    void vertexAttribIOffset(GLuint buffer, GLuint index, GLuint size, GLenum type,
                             GLsizei stride, GLintptr offset) {
        glVertexArrayVertexAttribIOffsetEXT(glId(), buffer, index, size, type, stride, offset);
    }

    void enableVertexAttrib(GLuint index) {
        glEnableVertexArrayAttribEXT(glId(), index);
    }

    void disableVertexAttrib(GLuint index) {
        glDisableVertexArrayAttribEXT(glId(), index);
    }
};

}
