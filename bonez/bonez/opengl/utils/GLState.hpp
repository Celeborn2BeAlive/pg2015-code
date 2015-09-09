#pragma once

#include <bonez/opengl/opengl.hpp>
#include <bonez/types.hpp>

namespace BnZ {

template<GLenum state>
struct GLState;

template<GLenum cap>
struct GLCapability {
    using ValueType = GLboolean;

    static ValueType get() {
        return glIsEnabled(cap);
    }

    static void set(ValueType value) {
        if(value) {
            glEnable(cap);
        } else {
            glDisable(cap);
        }
    }
};

template<>
struct GLState<GL_DEPTH_TEST>: public GLCapability<GL_DEPTH_TEST> {
};

template<>
struct GLState<GL_BLEND>: public GLCapability<GL_BLEND> {
};

template<>
struct GLState<GL_RASTERIZER_DISCARD>: public GLCapability<GL_RASTERIZER_DISCARD> {
};

template<>
struct GLState<GL_VIEWPORT> {
    using ValueType = Vec4f;

    static ValueType get() {
        ValueType value;
        glGetFloatv(GL_VIEWPORT, value_ptr(value));
        return value;
    }

    static void set(ValueType value) {
        glViewport(GLint(value.x), GLint(value.y), GLsizei(value.z), GLsizei(value.w));
    }
};

template<>
struct GLState<GL_DRAW_FRAMEBUFFER_BINDING> {
    using ValueType = GLint;

    static ValueType get() {
        ValueType value;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
        return value;
    }

    static void set(ValueType value) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, value);
    }
};

template<>
struct GLState<GL_READ_FRAMEBUFFER_BINDING> {
    using ValueType = GLint;

    static ValueType get() {
        ValueType value;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
        return value;
    }

    static void set(ValueType value) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, value);
    }
};

template<GLenum state>
typename GLState<state>::ValueType getGLState() {
    return GLState<state>::get();
}

template<GLenum state>
void setGLState(typename GLState<state>::ValueType value) {
    GLState<state>::set(value);
}

}
