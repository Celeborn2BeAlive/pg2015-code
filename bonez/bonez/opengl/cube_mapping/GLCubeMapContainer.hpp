#pragma once

#include <vector>
#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/utils/CubeMapUtils.hpp>

namespace BnZ {

struct GLCubeMapContainerUniform;

static std::string getCubeMapSphereTextureShader(const std::string& ext) {
    return "cube_mapping/cubeMapSphereTexture" + ext;
}

template<uint32_t nChannelCount, bool bHasDepthBuffer = true>
class GLCubeMapContainer {
public:
    static const uint32_t ChannelCount = nChannelCount;
    static const bool HasDepthBuffer = bHasDepthBuffer;
    static const uint32_t ColorBufferCount =  HasDepthBuffer ? ChannelCount - 1 : ChannelCount;

private:
    void initFBO() {
        m_FBO = GLFramebufferObject();
        for (auto i = 0u; i < ChannelCount; ++i) {
            glNamedFramebufferTextureEXT(m_FBO.glId(), m_Attachments[i],
                m_CubeMapArrays[i].glId(), 0);
        }

        GLenum status = glCheckNamedFramebufferStatusEXT(m_FBO.glId(), GL_DRAW_FRAMEBUFFER);
        if (GL_FRAMEBUFFER_COMPLETE != status) {
            std::cerr << GLFramebufferErrorString(status) << std::endl;
            return;
        }
    }
public:
    static std::string getSphereTextureShader(const std::string& ext) {
        return getCubeMapSphereTextureShader(ext);
    }

    using Uniforms = GLCubeMapContainerUniform;

    GLCubeMapContainer() = default;

    GLCubeMapContainer(uint32_t res, uint32_t count):
        m_nSize(count), m_nRes(res) {

        for(auto i = 0u; i < ChannelCount; ++i) {
            GLenum internalFormat;

            if(HasDepthBuffer && i == ColorBufferCount) {
                internalFormat = GL_DEPTH_COMPONENT32F;
                m_Attachments[i] = GL_DEPTH_ATTACHMENT;
            } else {
                internalFormat = GL_RGBA32F;
                m_Attachments[i] = GL_COLOR_ATTACHMENT0 + i;
            }

            m_CubeMapArrays[i] = GLTextureCubeMapArray();
            m_CubeMapArrays[i].setStorage(
                        1, internalFormat,
                        res, res,
                        count);

            m_CubeMapArrays[i].setMinFilter(GL_NEAREST);
            m_CubeMapArrays[i].setMagFilter(GL_NEAREST);
            m_CubeMapArrays[i].setWrapS(GL_CLAMP_TO_EDGE);
            m_CubeMapArrays[i].setWrapT(GL_CLAMP_TO_EDGE);

            //m_CubeMapArrays[i].makeTextureHandleResident();
        }
    }

    size_t size() const {
        return m_nSize;
    }

    uint32_t getResolution() const {
        return m_nRes;
    }

    GLCubeMapContainer(GLCubeMapContainer&& rvalue) :
        m_nSize(rvalue.m_nSize),
        m_nRes(rvalue.m_nRes),
        m_FBO(std::move(rvalue.m_FBO)) {
        for (auto i = 0u; i < 6; ++i) {
            m_FaceProjectionMatrices[i] = std::move(rvalue.m_FaceProjectionMatrices[i]);
        }
        for(auto i = 0u; i < ChannelCount; ++i) {
            m_Attachments[i] = rvalue.m_Attachments[i];
            m_CubeMapArrays[i] = std::move(rvalue.m_CubeMapArrays[i]);
        }
    }

    GLCubeMapContainer& operator =(GLCubeMapContainer&& rvalue) {
        m_nSize = rvalue.m_nSize;
        m_nRes = rvalue.m_nRes;
        m_FBO = std::move(rvalue.m_FBO);
        for (auto i = 0u; i < 6; ++i) {
            m_FaceProjectionMatrices[i] = std::move(rvalue.m_FaceProjectionMatrices[i]);
        }
        for(auto i = 0u; i < ChannelCount; ++i) {
            m_Attachments[i] = rvalue.m_Attachments[i];
            m_CubeMapArrays[i] = std::move(rvalue.m_CubeMapArrays[i]);
        }
        return *this;
    }

    void setDrawBuffers() {
        // Specify the outputs of the fragment shader
        glDrawBuffers(ColorBufferCount, m_Attachments);
    }

    void bindForDrawing() {
        if (!m_FBO) {
            initFBO();
        }

        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, m_FBO.glId());
    }

    const GLTextureCubeMapArray& getMapArray(uint32_t idx) const {
        return m_CubeMapArrays[idx];
    }

    GLTextureCubeMapArray& getMapArray(uint32_t idx) {
        return m_CubeMapArrays[idx];
    }

    uint32_t getDepthMapChannelIndex() const {
        static_assert(HasDepthBuffer,
                      "CubeMapContainer does not have a depth buffer.");
        return ColorBufferCount;
    }

    const GLTextureCubeMapArray& getDepthMapArray() const {
        return m_CubeMapArrays[getDepthMapChannelIndex()];
    }

    void computeFaceProjMatrices(float zNear, float zFar) {
        CubeMapUtils::computeFaceProjMatrices(zNear, zFar, m_FaceProjectionMatrices);
    }

    const Mat4f* getFaceProjMatrices() const {
        return m_FaceProjectionMatrices;
    }

    void makeResident() {
        for(auto& texture: m_CubeMapArrays) {
            texture.makeTextureHandleResident();
        }
    }

    void makeNonResident() {
        for(auto& texture: m_CubeMapArrays) {
            texture.makeTextureHandleNonResident();
        }
    }

private:
    uint32_t m_nSize = 0u;
    uint32_t m_nRes = 0u;
    GLFramebufferObject m_FBO = GLFramebufferObject(0);
    GLTextureCubeMapArray m_CubeMapArrays[ChannelCount];
    GLenum m_Attachments[ChannelCount];
    Mat4f m_FaceProjectionMatrices[6];
};

struct GLCubeMapContainerUniform {
    GLUniform<GLSLSamplerCubeArrayf> texture;

    GLCubeMapContainerUniform(const GLProgram& program):
        texture(program, "uCubeMapContainer.texture") {
    }

    template<uint32_t ChannelCount, bool HasDepthTexture>
    void set(uint32_t channelIdx, GLuint textureUnitIndex,
             const GLCubeMapContainer<ChannelCount, HasDepthTexture>& container) {
        container.getMapArray(channelIdx).bind(textureUnitIndex);
        texture.set(textureUnitIndex);
    }
};

}
