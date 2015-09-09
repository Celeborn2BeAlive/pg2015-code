#pragma once

#include "bonez/opengl/opengl.hpp"
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

#define BNZ_SHADERSRC(str) #str

namespace BnZ {

class GLShader {
    GLuint m_GLId;
    typedef std::unique_ptr<char[]> CharBuffer;
public:
    GLShader(GLenum type) : m_GLId(glCreateShader(type)) {
    }

    ~GLShader() {
        glDeleteShader(m_GLId);
    }

    GLShader(const GLShader&) = delete;

    GLShader& operator =(const GLShader&) = delete;

    GLShader(GLShader&& rvalue) : m_GLId(rvalue.m_GLId) {
        rvalue.m_GLId = 0;
    }

    GLShader& operator =(GLShader&& rvalue) {
        this->~GLShader();
        m_GLId = rvalue.m_GLId;
        rvalue.m_GLId = 0;
        return *this;
    }

    GLuint glId() const {
        return m_GLId;
    }

    void setSource(const GLchar* src) {
        glShaderSource(m_GLId, 1, &src, 0);
    }

    void setSource(const std::string& src) {
        setSource(src.c_str());
    }

    bool compile() {
        glCompileShader(m_GLId);
        return getCompileStatus();
    }

    bool getCompileStatus() const {
        GLint status;
        glGetShaderiv(m_GLId, GL_COMPILE_STATUS, &status);
        return status == GL_TRUE;
    }

    std::string getInfoLog() const {
        GLint logLength;
        glGetShaderiv(m_GLId, GL_INFO_LOG_LENGTH, &logLength);

        CharBuffer buffer(new char[logLength]);
        glGetShaderInfoLog(m_GLId, logLength, 0, buffer.get());

        return std::string(buffer.get());
    }
};

inline std::string loadShaderSource(const std::string& filepath) {
    std::ifstream input(filepath);
    if(!input) {
        std::cerr << "Unable to open file " << filepath << std::endl;
        throw std::runtime_error("Unable to open file " + filepath);
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}

template<typename StringType>
GLShader compileShader(GLenum type, StringType&& src) {
    GLShader shader(type);
    shader.setSource(std::forward<StringType>(src));
    if (!shader.compile()) {
        std::cerr << shader.getInfoLog() << std::endl;
        throw std::runtime_error(shader.getInfoLog());
    }
    return shader;
}

}
