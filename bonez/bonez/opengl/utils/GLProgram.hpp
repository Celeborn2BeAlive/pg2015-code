#pragma once

#include "GLShader.hpp"
#include "bonez/opengl/opengl.hpp"
#include <iostream>

namespace BnZ {

class GLProgram {
    GLuint m_GLId;
    typedef std::unique_ptr<char[]> CharBuffer;
public:
    GLProgram() : m_GLId(glCreateProgram()) {
    }

    ~GLProgram() {
        glDeleteProgram(m_GLId);
    }

    GLProgram(const GLProgram&) = delete;

    GLProgram& operator =(const GLProgram&) = delete;

    GLProgram(GLProgram&& rvalue) : m_GLId(rvalue.m_GLId) {
        rvalue.m_GLId = 0;
    }

    GLProgram& operator =(GLProgram&& rvalue) {
        this->~GLProgram();
        m_GLId = rvalue.m_GLId;
        rvalue.m_GLId = 0;
        return *this;
    }

    GLuint glId() const {
        return m_GLId;
    }

    void attachShader(const GLShader& shader) {
        glAttachShader(m_GLId, shader.glId());
    }

    bool link() {
        glLinkProgram(m_GLId);
        return getLinkStatus();
    }

    bool getLinkStatus() const {
        GLint linkStatus;
        glGetProgramiv(m_GLId, GL_LINK_STATUS, &linkStatus);
        return linkStatus == GL_TRUE;
    }

    std::string getInfoLog() const {
        GLint logLength;
        glGetProgramiv(m_GLId, GL_INFO_LOG_LENGTH, &logLength);

        CharBuffer buffer(new char[logLength]);
        glGetProgramInfoLog(m_GLId, logLength, 0, buffer.get());

        return std::string(buffer.get());
    }

    void use() const {
        glUseProgram(m_GLId);
    }

    GLint getUniformLocation(const GLchar* name) const {
        GLint location = glGetUniformLocation(m_GLId, name);
        return location;
    }

    GLint getAttribLocation(const GLchar* name) const {
        GLint location = glGetAttribLocation(m_GLId, name);
        return location;
    }

    void bindAttribLocation(GLuint index, const GLchar* name) const {
        glBindAttribLocation(m_GLId, index, name);
    }
};

inline GLProgram buildProgram(std::initializer_list<GLShader> shaders) {
    GLProgram program;
    for (const auto& shader : shaders) {
        program.attachShader(shader);
    }

    if (!program.link()) {
        std::cerr << program.getInfoLog() << std::endl;
        throw std::runtime_error(program.getInfoLog());
    }

    return program;
}

template<typename VSrc, typename FSrc>
GLProgram buildProgram(VSrc&& vsrc, FSrc&& fsrc) {
    GLShader vs = compileShader(GL_VERTEX_SHADER, std::forward<VSrc>(vsrc));
    GLShader fs = compileShader(GL_FRAGMENT_SHADER, std::forward<FSrc>(fsrc));

    return buildProgram({ std::move(vs), std::move(fs) });
}

template<typename VSrc, typename GSrc, typename FSrc>
GLProgram buildProgram(VSrc&& vsrc, GSrc&& gsrc, FSrc&& fsrc) {
    GLShader vs = compileShader(GL_VERTEX_SHADER, std::forward<VSrc>(vsrc));
    GLShader gs = compileShader(GL_GEOMETRY_SHADER, std::forward<GSrc>(gsrc));
    GLShader fs = compileShader(GL_FRAGMENT_SHADER, std::forward<FSrc>(fsrc));

    return buildProgram({ std::move(vs), std::move(gs), std::move(fs) });
}

template<typename CSrc>
GLProgram buildComputeProgram(CSrc&& src) {
    GLShader cs = compileShader(GL_COMPUTE_SHADER, std::forward<CSrc>(src));

    return buildProgram({ std::move(cs) });;
}

}
