#pragma once

#include <iostream>
#include "GLProgram.hpp"
#include "GLObject.hpp"
#include "GLTexture.hpp"
#include "GLBindlessTexture.hpp"
#include "GLNVShaderBufferLoad.hpp"

namespace BnZ {

template<typename T>
struct GLUniformHelper;

#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE> { \
        static void set(GLuint programID, GLint location, TYPE v0) { \
            glProgram##FUNCTION(programID, location, v0); \
        } \
        static void set(GLint location, TYPE v0) { \
            gl##FUNCTION(location, v0); \
        } \
    }

GEN_UNIFORM_HELPER(float, Uniform1f);
GEN_UNIFORM_HELPER(int, Uniform1i);
GEN_UNIFORM_HELPER(unsigned int, Uniform1ui);
GEN_UNIFORM_HELPER(bool, Uniform1i);
GEN_UNIFORM_HELPER(GLuint64, Uniform1ui64NV);

#undef GEN_UNIFORM_HELPER

// For glm vector types:
#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE> { \
        static void set(GLuint programID, GLint location, TYPE value) { \
            glProgram##FUNCTION(programID, location, 1, value_ptr(value)); \
        } \
        static void set(GLint location, TYPE value) { \
            gl##FUNCTION(location, 1, value_ptr(value)); \
        } \
    }

GEN_UNIFORM_HELPER(Vec2f, Uniform2fv);
GEN_UNIFORM_HELPER(Vec3f, Uniform3fv);
GEN_UNIFORM_HELPER(Vec4f, Uniform4fv);
GEN_UNIFORM_HELPER(Vec2i, Uniform2iv);
GEN_UNIFORM_HELPER(Vec3i, Uniform3iv);
GEN_UNIFORM_HELPER(Vec4i, Uniform4iv);
GEN_UNIFORM_HELPER(Vec2u, Uniform2uiv);
GEN_UNIFORM_HELPER(Vec3u, Uniform3uiv);
GEN_UNIFORM_HELPER(Vec4u, Uniform4uiv);

#undef GEN_UNIFORM_HELPER

// For glm matrix types:
#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE> { \
        static void set(GLuint programID, GLint location, TYPE value) { \
            glProgram##FUNCTION(programID, location, 1, GL_FALSE, value_ptr(value)); \
        } \
        static void set(GLint location, TYPE value) { \
            gl##FUNCTION(location, 1, GL_FALSE, value_ptr(value)); \
        } \
    }

GEN_UNIFORM_HELPER(Mat2f, UniformMatrix2fv);
GEN_UNIFORM_HELPER(Mat3f, UniformMatrix3fv);
GEN_UNIFORM_HELPER(Mat4f, UniformMatrix4fv);
GEN_UNIFORM_HELPER(Mat2x3f, UniformMatrix2x3fv);
GEN_UNIFORM_HELPER(Mat3x2f, UniformMatrix3x2fv);
GEN_UNIFORM_HELPER(Mat2x4f, UniformMatrix2x4fv);
GEN_UNIFORM_HELPER(Mat4x2f, UniformMatrix4x2fv);
GEN_UNIFORM_HELPER(Mat3x4f, UniformMatrix3x4fv);
GEN_UNIFORM_HELPER(Mat4x3f, UniformMatrix4x3fv);

#undef GEN_UNIFORM_HELPER

// For simple array types
#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE[]> { \
        static void set(GLuint programID, GLint location, GLsizei count, const TYPE* value) { \
            glProgram##FUNCTION(programID, location, count, value); \
        } \
        static void set(GLint location, GLsizei count, const TYPE* value) { \
            gl##FUNCTION(location, count, value); \
        } \
    }

GEN_UNIFORM_HELPER(float, Uniform1fv);
GEN_UNIFORM_HELPER(int, Uniform1iv);
GEN_UNIFORM_HELPER(unsigned int, Uniform1uiv);
GEN_UNIFORM_HELPER(GLuint64, Uniform1ui64vNV);

#undef GEN_UNIFORM_HELPER

// For glm array types:
#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE[]> { \
        static void set(GLuint programID, GLint location, GLsizei count, const TYPE* value) { \
            glProgram##FUNCTION(programID, location, count, value_ptr(value[0])); \
        } \
        static void set(GLint location, GLsizei count, const TYPE* value) { \
            gl##FUNCTION(location, count, value_ptr(value[0])); \
        } \
    }

GEN_UNIFORM_HELPER(Vec2f, Uniform2fv);
GEN_UNIFORM_HELPER(Vec3f, Uniform3fv);
GEN_UNIFORM_HELPER(Vec4f, Uniform4fv);
GEN_UNIFORM_HELPER(Vec2i, Uniform2iv);
GEN_UNIFORM_HELPER(Vec3i, Uniform3iv);
GEN_UNIFORM_HELPER(Vec4i, Uniform4iv);
GEN_UNIFORM_HELPER(Vec2u, Uniform2uiv);
GEN_UNIFORM_HELPER(Vec3u, Uniform3uiv);
GEN_UNIFORM_HELPER(Vec4u, Uniform4uiv);

#undef GEN_UNIFORM_HELPER

// For glm array matrix types:
#define GEN_UNIFORM_HELPER(TYPE, FUNCTION) \
    template<> \
    struct GLUniformHelper<TYPE[]> { \
        static void set(GLuint programID, GLint location, GLsizei count, const TYPE* value) { \
            glProgram##FUNCTION(programID, location, count, GL_FALSE, value_ptr(value[0])); \
        } \
        static void set(GLint location, GLsizei count, const TYPE* value) { \
            gl##FUNCTION(location, count, GL_FALSE, value_ptr(value[0])); \
        } \
    }

GEN_UNIFORM_HELPER(Mat2f, UniformMatrix2fv);
GEN_UNIFORM_HELPER(Mat3f, UniformMatrix3fv);
GEN_UNIFORM_HELPER(Mat4f, UniformMatrix4fv);
GEN_UNIFORM_HELPER(Mat2x3f, UniformMatrix2x3fv);
GEN_UNIFORM_HELPER(Mat3x2f, UniformMatrix3x2fv);
GEN_UNIFORM_HELPER(Mat2x4f, UniformMatrix2x4fv);
GEN_UNIFORM_HELPER(Mat4x2f, UniformMatrix4x2fv);
GEN_UNIFORM_HELPER(Mat3x4f, UniformMatrix3x4fv);
GEN_UNIFORM_HELPER(Mat4x3f, UniformMatrix4x3fv);

#undef GEN_UNIFORM_HELPER

template<typename T>
struct GLUniformHelper<GLBufferAddress<T>> {
    static void set(GLuint programID, GLint location, GLBufferAddress<T> value) {
        glProgramUniform1ui64NV(programID, location, GLuint64(value));
    }
    static void set(GLint location, GLBufferAddress<T> value) {
        glUniform1ui64NV(location, GLuint64(value));
    }
};

template<typename T, typename TextureType>
struct GLSLSampler;

//using GLSLSampler1Df = GLSLSampler<float, GLTexture1D>;
using GLSLSampler2Df = GLSLSampler<float, GLTexture2D>;
using GLSLSampler3Df = GLSLSampler<float, GLTexture3D>;
using GLSLSamplerCubef = GLSLSampler<float, GLTextureCubeMap>;
//using GLSLSampler2DRectf = GLSLSampler<float, GLTexture2DRect>;
//using GLSLSampler1DArrayf = GLSLSampler<float, GLTexture1DArray>;
using GLSLSampler2DArrayf = GLSLSampler<float, GLTexture2DArray>;
using GLSLSamplerCubeArrayf = GLSLSampler<float, GLTextureCubeMapArray>;
//using GLSLSamplerBufferf = GLSLSampler<float, GLTextureBuffer>;

template<typename T, typename TextureType>
struct GLUniformHelper<GLSLSampler<T, TextureType>> {
    static void set(GLuint programID, GLint location, GLuint textureUnitIndex) {
        glProgramUniform1i(programID, location, textureUnitIndex);
    }
    static void set(GLint location, GLuint textureUnitIndex) {
        glUniform1i(location, textureUnitIndex);
    }
    static void set(GLuint programID, GLint location, GLTextureHandle<TextureType::TEXTURE_TYPE> value) {
        glProgramUniform1ui64NV(programID, location, GLuint64(value));
    }
    static void set(GLint location, GLTextureHandle<TextureType::TEXTURE_TYPE> value) {
        glUniform1ui64NV(location, GLuint64(value));
    }
};

template<typename T, typename TextureType>
struct GLSLImage;

//using GLSLImage1Df = GLSLImage<float, GLTexture1D>;
using GLSLImage2Df = GLSLImage<float, GLTexture2D>;
using GLSLImage3Df = GLSLImage<float, GLTexture3D>;
using GLSLImageCubef = GLSLImage<float, GLTextureCubeMap>;
//using GLSLImage2DRectf = GLSLImage<float, GLTexture2DRect>;
//using GLSLImage1DArrayf = GLSLImage<float, GLTexture1DArray>;
using GLSLImage2DArrayf = GLSLImage<float, GLTexture2DArray>;
using GLSLImageCubeArrayf = GLSLImage<float, GLTextureCubeMapArray>;
//using GLSLImageBufferf = GLSLImage<float, GLTextureBuffer>;

template<typename T, typename TextureType>
struct GLUniformHelper<GLSLImage<T, TextureType>> {
    static void set(GLuint programID, GLint location, GLuint textureUnitIndex) {
        glProgramUniform1i(programID, location, textureUnitIndex);
    }
    static void set(GLint location, GLuint textureUnitIndex) {
        glUniform1i(location, textureUnitIndex);
    }
    static void set(GLuint programID, GLint location, GLImageHandle<TextureType::TEXTURE_TYPE> value) {
        glProgramUniform1ui64NV(programID, location, GLuint64(value));
    }
    static void set(GLint location, GLImageHandle<TextureType::TEXTURE_TYPE> value) {
        glUniform1ui64NV(location, GLuint64(value));
    }
};

template<typename T>
class GLUniform {
public:
    GLUniform(const GLProgram& program, const GLchar* name):
        m_nLocation(program.getUniformLocation(name)) {
        if(m_nLocation < 0) {
            std::clog << "WARNING: uniform '" << name << "' not found in the program "
                << program.glId() << std::endl;
        }
    }

    GLint location() const {
        return m_nLocation;
    }

    template<typename U>
    void set(U value) {
        GLUniformHelper<T>::set(m_nLocation, value);
    }

    template<typename U>
    void set(const GLProgram& program, U value) {
        GLUniformHelper<T>::set(program.glId(), m_nLocation, value);
    }

private:
    GLint m_nLocation;
};

template<typename T>
class GLUniform<T[]> {
public:
    GLUniform(const GLProgram& program, const GLchar* name) :
        m_nLocation(program.getUniformLocation(name)) {
        if(m_nLocation < 0) {
            std::clog << "WARNING: uniform '" << name << "' not found in the program "
                << program.glId() << std::endl;
        }
    }

    GLint location() const {
        return m_nLocation;
    }

    void set(GLsizei count, const T* value) {
        GLUniformHelper<T[]>::set(m_nLocation, count, value);
    }

    void set(const GLProgram& program, GLsizei count, const T* value) {
        GLUniformHelper<T[]>::set(program.glId(), m_nLocation, count, value);
    }

private:
    GLint m_nLocation;
};

#define BNZ_GLUNIFORM(PROGRAM, TYPE, NAME) GLUniform<TYPE> NAME { PROGRAM, #NAME }

}
