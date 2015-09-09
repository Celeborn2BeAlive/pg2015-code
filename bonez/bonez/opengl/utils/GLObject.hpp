#pragma once

#include "bonez/opengl/opengl.hpp"

namespace BnZ {

template<typename Factory>
class GLObject {
    GLuint m_GLId;
public:
    explicit GLObject(GLuint id) :
        m_GLId(id) {
    }

    GLObject() {
        Factory::createObjects(1, &m_GLId);
    }

    // Only works for textures and queries, that need a target to be created
//    GLObject(GLenum target) {
//        Factory::createObjects(target, 1, &m_GLId);
//    }

    ~GLObject() {
        Factory::deleteObjects(1, &m_GLId);
    }

    GLObject(const GLObject&) = delete;

    GLObject& operator =(const GLObject&) = delete;

    GLObject(GLObject&& rvalue): m_GLId(rvalue.m_GLId) {
        rvalue.m_GLId = 0;
    }

    GLObject& operator =(GLObject&& rvalue) {
        this->~GLObject();
        m_GLId = rvalue.m_GLId;
        rvalue.m_GLId = 0;
        return *this;
    }

    explicit operator bool() const {
        return m_GLId != 0;
    }

    GLuint glId() const {
        return m_GLId;
    }
};

struct GLBufferFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenBuffers(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateBuffers(n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteBuffers(n, objects);
    }
};
using GLBufferObject = GLObject<GLBufferFactory>;

template<GLenum target>
struct GLTextureFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenTextures(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateTextures(target, n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteTextures(n, objects);
    }
};
template<GLenum target>
using GLTextureObject = GLObject<GLTextureFactory<target>>;

struct GLFramebufferFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenFramebuffers(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateFramebuffers(n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteFramebuffers(n, objects);
    }
};
using GLFramebufferObject = GLObject<GLFramebufferFactory>;

struct GLTransformFeedbackFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenTransformFeedbacks(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateTransformFeedbacks(n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteTransformFeedbacks(n, objects);
    }
};
using GLTransformFeedbackObject = GLObject<GLTransformFeedbackFactory>;

struct GLVertexArrayFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenVertexArrays(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateVertexArrays(n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteVertexArrays(n, objects);
    }
};
using GLVertexArrayObject = GLObject<GLVertexArrayFactory>;

template<GLenum target>
struct GLQueryFactory {
    static void genObjects(GLsizei n, GLuint* objects) {
        glGenQueries(n, objects);
    }

    static void createObjects(GLsizei n, GLuint* objects) {
        glCreateQueries(target, n, objects);
    }

    static void deleteObjects(GLsizei n, GLuint* objects) {
        glDeleteQueries(n, objects);
    }
};
template<GLenum target>
using GLQueryObject = GLObject<GLQueryFactory<target>>;

template<typename Factory>
GLuint genGLObject() {
    GLuint id = 0;
    Factory::genObjects(1, &id);
    return id;
}

template<typename Factory>
GLuint createGLObject() {
    GLuint id = 0;
    Factory::createObjects(1, &id);
    return id;
}

template<typename Factory>
GLuint createGLObject(GLenum target) {
    GLuint id = 0;
    Factory::createObjects(target, 1, &id);
    return id;
}

//#undef GL_OBJECT

}
