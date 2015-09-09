#pragma once

#include <vector>
#include <cassert>
#include "GLObject.hpp"

namespace BnZ {

struct DontCreateBufferObjectFlag {};

template<typename T>
class GLBufferBase: GLBufferObject {
protected:
    size_t m_Size = 0;

    GLBufferBase() = default;

    GLBufferBase(DontCreateBufferObjectFlag flag): GLBufferObject(0) {
    }

    GLBufferBase(size_t size): m_Size(size) {
    }

    GLBufferBase(GLBufferBase&& rvalue) : GLBufferObject(std::move(rvalue)),
        m_Size(rvalue.m_Size) {
    }

    GLBufferBase& operator =(GLBufferBase&& rvalue) {
        GLBufferObject::operator=(std::move(rvalue));
        m_Size = rvalue.m_Size;
        return *this;
    }

    ~GLBufferBase() = default;
public:
    using GLBufferObject::glId;

    size_t size() const {
        return m_Size;
    }

    size_t byteSize() const {
        return m_Size * sizeof(T);
    }

    void setSubData(uint32_t offset, size_t count, const T* data) {
        assert(offset + count <= m_Size);
        glNamedBufferSubDataEXT(glId(), offset * sizeof(T), count * sizeof(T), data);
    }

    void setSubData(uint32_t offset, const std::vector<T>& data) {
        setSubData(offset, data.size(), data.data());
    }

    void setSubData(const std::vector<T>& data) {
        setSubData(0, data);
    }

    void bind(GLenum target) const {
        glBindBuffer(target, glId());
    }

    void bindBase(GLenum target, GLuint index) const {
        glBindBufferBase(target, index, glId());
    }

    void bindRange(GLenum target, GLuint index, size_t offset, size_t size) const {
        glBindBufferRange(target, index, glId(), offset * sizeof(T), size * sizeof(T));
    }

    T* map(GLenum access) {
        return (T*) glMapNamedBufferEXT(glId(), access);
    }

    T* mapReadOnly() const {
        return (T*) glMapNamedBufferEXT(glId(), GL_READ_ONLY);
    }

    void unmap() const {
        glUnmapNamedBufferEXT(glId());
    }

    void getData(size_t offset, size_t count, T* data) {
        glGetNamedBufferSubDataEXT(glId(), offset * sizeof(T), count * sizeof(T), data);
    }

    void getData(size_t count, T* data) {
        getData(0, count, data);
    }

    void getData(size_t offset, std::vector<T>& data) {
        getData(offset, data.size(), data.data());
    }

    void getData(std::vector<T>& data) {
        getData(0, data);
    }
};

template<typename T>
class GLBuffer: public GLBufferBase<T> {
    using GLBufferBase<T>::m_Size;
public:
    using GLBufferBase<T>::glId;

    void setSize(size_t count, GLenum usage) {
        m_Size = count;
        glNamedBufferDataEXT(glId(), m_Size * sizeof(T), nullptr, usage);
    }

    void setData(size_t count, const T* data, GLenum usage) {
        m_Size = count;
        glNamedBufferDataEXT(glId(), m_Size * sizeof(T), data, usage);
    }

    void setData(const std::vector<T>& data, GLenum usage) {
        setData(data.size(), data.data(), usage);
    }
};

template<typename T>
class GLBufferStorage: public GLBufferBase<T> {
public:
    using GLBufferBase<T>::glId;

    // Create a null buffer storage
    GLBufferStorage():
        GLBufferBase<T>(DontCreateBufferObjectFlag{}) {
    }

    GLBufferStorage(size_t size, const T* data, GLbitfield flags = 0):
        GLBufferBase<T>(size) {
        glNamedBufferStorageEXT(glId(), GLBufferBase<T>::size() * sizeof(T), data, flags);
    }

    GLBufferStorage(const std::vector<T>& data, GLbitfield flags = 0):
        GLBufferStorage(data.size(), data.data(), flags) {
    }

    T* mapRange(GLintptr offset, GLsizei length, GLbitfield access) {
        assert(offset + length <= GLBufferBase<T>::size());
        return (T*) glMapNamedBufferRangeEXT(glId(), offset, length, access);
    }
};

template<typename T>
inline GLBufferStorage<T> genBufferStorage(const T& value, GLbitfield flags = 0) {
    return GLBufferStorage<T>(sizeof(value), &value, flags);
}

template<typename T>
inline GLBufferStorage<T> genBufferStorage(size_t size, const T* data,
                                             GLbitfield flags = 0) {
    return GLBufferStorage<T>(size, data, flags);
}

template<typename T>
inline GLBufferStorage<T> genBufferStorage(const std::vector<T>& data, GLbitfield flags = 0) {
    return GLBufferStorage<T>(data, flags);
}

}
