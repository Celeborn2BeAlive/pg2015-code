#pragma once

#include "bonez/opengl/utils/GLFramebuffer.hpp"
#include "bonez/opengl/utils/GLUniform.hpp"

namespace BnZ {

enum GBufferTextureType {
    GBUFFER_NORMAL_DEPTH,
    GBUFFER_DIFFUSE,
    GBUFFER_GLOSSY_SHININESS,
    GBUFFER_TEXTURE_COUNT
};

struct GLGBuffer: public GLFramebuffer2D<GBUFFER_TEXTURE_COUNT> {
    void init(uint32_t width, uint32_t height) {
        GLenum internalFormats[] = { GL_RGBA32F, GL_RGBA32F, GL_RGBA32F };
        GLFramebuffer2D<GBUFFER_TEXTURE_COUNT>::init(width, height,
            internalFormats,
            GL_DEPTH_COMPONENT32F);
    }

    void init(const Vec2u& size) {
        return init(size.x, size.y);
    }
};

struct GLGBufferUniform {
    GLUniform<GLSLSampler2Df> normalDepthSampler;
    GLUniform<GLSLSampler2Df> diffuseSampler;
    GLUniform<GLSLSampler2Df> glossyShininessSampler;

    GLGBufferUniform(const GLProgram& program):
        normalDepthSampler(program, "uGBuffer.normalDepthSampler"),
        diffuseSampler(program, "uGBuffer.diffuseSampler"),
        glossyShininessSampler(program, "uGBuffer.glossyShininessSampler") {
    }

    void set(const GLGBuffer& gbuffer, GLuint normalDepthTexUnit,
             GLuint diffuseTextUnit, GLuint glossyShininessTexUnit) {
        gbuffer.getColorBuffer(GBUFFER_NORMAL_DEPTH).bind(normalDepthTexUnit);
        gbuffer.getColorBuffer(GBUFFER_DIFFUSE).bind(diffuseTextUnit);
        gbuffer.getColorBuffer(GBUFFER_GLOSSY_SHININESS).bind(glossyShininessTexUnit);

        normalDepthSampler.set(normalDepthTexUnit);
        diffuseSampler.set(diffuseTextUnit);
        glossyShininessSampler.set(glossyShininessTexUnit);
    }
};

}
