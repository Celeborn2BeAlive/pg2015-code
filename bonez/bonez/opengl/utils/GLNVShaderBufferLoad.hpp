#pragma once

#include "GLBuffer.hpp"

namespace BnZ {

template<typename T>
class GLBufferAddress {
    GLuint64 m_nAddress;
public:
    explicit GLBufferAddress(GLuint64 address = 0u):
        m_nAddress(address) {
    }

    explicit GLBufferAddress(const GLBuffer<T>& buffer) {
        glGetNamedBufferParameterui64vNV(buffer.glId(),
                                         GL_BUFFER_GPU_ADDRESS_NV,
                                         &m_nAddress);
    }

    explicit operator GLuint64() const {
        return m_nAddress;
    }

    explicit operator bool() const {
        return m_nAddress != 0;
    }

    template<typename Integer>
    GLBufferAddress& operator +=(Integer offset) {
        m_nAddress += sizeof(T) * offset;
        return *this;
    }

    template<typename Integer>
    friend GLBufferAddress operator +(GLBufferAddress addr, Integer offset) {
        return addr += offset;
    }

    template<typename Integer>
    friend GLBufferAddress operator +(Integer offset, GLBufferAddress addr) {
        return addr + offset;
    }
};

template<typename T>
void makeResident(const GLBuffer<T>& buffer, GLenum access) {
    glMakeNamedBufferResidentNV(buffer.glId(), access);
}

template<typename T>
void makeNonResident(const GLBuffer<T>& buffer) {
    glMakeNamedBufferNonResidentNV(buffer.glId());
}

}
