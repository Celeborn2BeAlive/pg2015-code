#pragma once

#include <vector>
#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/utils/CubeMapUtils.hpp>

namespace BnZ {

inline void getFaceVirtualViewports(uint32_t res, Vec4f viewports[6]) {
    viewports[0] = Vec4f(0, 0, res, res);
    viewports[1] = Vec4f(0, res, res, res);
    viewports[2] = Vec4f(res, 0, res, res);
    viewports[3] = Vec4f(res, res, res, res);
    viewports[4] = Vec4f(2 * res, 0, res, res);
    viewports[5] = Vec4f(2 * res, res, res, res);
}

inline GLTextureCubeMap createIndirectionCubeMap(uint32_t res) {
    Vec4f viewports[6];
    getFaceVirtualViewports(res, viewports);

    GLTextureCubeMap indirectionCubeMap;

    std::vector<Vec2f> indirectionMap(res * res);
    float scale = 1.f / res;
    for(auto j = 0u; j < 6; ++j) {
        auto viewport = viewports[j] / Vec4f(3 * res, 2 * res, 3 * res, 2 * res);
        for(auto y = 0u; y < res; ++y) {
            for(auto x = 0u; x < res; ++x) {
                auto uv = scale * Vec2f(x + 0.5f, y + 0.5f);
                indirectionMap[x + y * res] = viewport.xy() + uv * viewport.zw();
            }
        }
        indirectionCubeMap.setImage(j, 0, GL_RG16F, res, res, 0,
                                   GL_RG, GL_FLOAT, indirectionMap.data());
    }
    indirectionCubeMap.setMinFilter(GL_NEAREST);
    indirectionCubeMap.setMagFilter(GL_NEAREST);
    indirectionCubeMap.setWrapS(GL_CLAMP_TO_EDGE);
    indirectionCubeMap.setWrapT(GL_CLAMP_TO_EDGE);

    indirectionCubeMap.makeTextureHandleResident();

    return indirectionCubeMap;
}

struct GLVirtualCubeMapContainerUniform;

inline std::string getVirtualCubeMapSphereTextureShader(const std::string& ext) {
    return "cube_mapping/virtualCubeMapSphereTexture" + ext;
}

/****
 * Texture layout:
 *  ______________
 * | -X | -Y | -Z |
 * |____|____|____|
 * | +X | +Y | +Z |
 * |____|____|____|
 *
 */
template<uint32_t nChannelCount, bool bHasDepthBuffer = true>
class GLVirtualCubeMapContainer {
public:
    static const uint32_t ChannelCount = nChannelCount;
    static const bool HasDepthBuffer = bHasDepthBuffer;
    static const uint32_t ColorBufferCount =  HasDepthBuffer ? ChannelCount - 1 : ChannelCount;

private:
    void initFBO() {
        m_FBO = GLFramebufferObject();
        for (auto i = 0u; i < ChannelCount; ++i) {
            glNamedFramebufferTextureEXT(m_FBO.glId(), m_Attachments[i],
                m_TextureArrays[i].glId(), 0);
        }

        GLenum status = glCheckNamedFramebufferStatusEXT(m_FBO.glId(), GL_DRAW_FRAMEBUFFER);
        if (GL_FRAMEBUFFER_COMPLETE != status) {
            std::cerr << GLFramebufferErrorString(status) << std::endl;
            return;
        }
    }
public:
    static std::string getSphereTextureShader(const std::string& ext) {
        return getVirtualCubeMapSphereTextureShader(ext);
    }

    using Uniforms = GLVirtualCubeMapContainerUniform;

    GLVirtualCubeMapContainer() = default;

    GLVirtualCubeMapContainer(uint32_t res, uint32_t count) {
        init(res, count);
    }

    void init(uint32_t res, uint32_t count) {
        m_nSize = count;
        m_nRes = res;

        for(auto i = 0u; i < ChannelCount; ++i) {
            GLenum internalFormat;

            if(HasDepthBuffer && i == ColorBufferCount) {
                internalFormat = GL_DEPTH_COMPONENT32F;
                m_Attachments[i] = GL_DEPTH_ATTACHMENT;
            } else {
                internalFormat = GL_RGBA32F;
                m_Attachments[i] = GL_COLOR_ATTACHMENT0 + i;
            }

            m_TextureArrays[i] = GLTexture2DArray();
            m_TextureArrays[i].setStorage(1, internalFormat,
                                     res * 3, res * 2,
                                     count);

            m_TextureArrays[i].setMinFilter(GL_LINEAR);
            m_TextureArrays[i].setMagFilter(GL_LINEAR);

            m_TextureArrays[i].makeTextureHandleResident();
        }

        getFaceVirtualViewports(res, m_FaceViewports);
        m_IndirectionTexture = createIndirectionCubeMap(res);
    }

    size_t size() const {
        return m_nSize;
    }

    uint32_t getResolution() const {
        return m_nRes;
    }

    const GLTextureCubeMap& getIndirectionTexture() const {
        return m_IndirectionTexture;
    }

    const Vec4f* getFaceViewports() const {
        return m_FaceViewports;
    }

    GLVirtualCubeMapContainer(GLVirtualCubeMapContainer&& rvalue) :
        m_nSize(rvalue.m_nSize),
        m_nRes(rvalue.m_nRes),
        m_FBO(std::move(rvalue.m_FBO)),
        m_IndirectionTexture(std::move(rvalue.m_IndirectionTexture)) {
        for (auto i = 0u; i < 6; ++i) {
            m_FaceViewports[i] = std::move(rvalue.m_FaceViewports[i]);
            m_FaceProjectionMatrices[i] = std::move(rvalue.m_FaceProjectionMatrices[i]);
        }
        for(auto i = 0u; i < ChannelCount; ++i) {
            m_TextureArrays[i] = std::move(rvalue.m_TextureArrays[i]);
            m_Attachments[i] = rvalue.m_Attachments[i];
        }
    }

    GLVirtualCubeMapContainer& operator =(GLVirtualCubeMapContainer&& rvalue) {
        m_nSize = rvalue.m_nSize;
        m_nRes = rvalue.m_nRes;
        m_FBO = std::move(rvalue.m_FBO);
        m_IndirectionTexture = std::move(rvalue.m_IndirectionTexture);
        for (auto i = 0u; i < 6; ++i) {
            m_FaceViewports[i] = std::move(rvalue.m_FaceViewports[i]);
            m_FaceProjectionMatrices[i] = std::move(rvalue.m_FaceProjectionMatrices[i]);
        }
        for(auto i = 0u; i < ChannelCount; ++i) {
            m_TextureArrays[i] = std::move(rvalue.m_TextureArrays[i]);
            m_Attachments[i] = rvalue.m_Attachments[i];
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

    const GLTexture2DArray& getMapArray(uint32_t idx) const {
        return m_TextureArrays[idx];
    }

    uint32_t getDepthMapChannelIndex() const {
        static_assert(HasDepthBuffer,
                      "VirtualCubeMapContainer does not have a depth buffer.");
        return ColorBufferCount;
    }

    const GLTexture2DArray& getDepthMapArray() const {
        return m_TextureArrays[getDepthMapChannelIndex()];
    }

    void computeFaceProjMatrices(float zNear, float zFar) {
        CubeMapUtils::computeFaceProjMatrices(zNear, zFar, m_FaceProjectionMatrices);
    }

    const Mat4f* getFaceProjMatrices() const {
        return m_FaceProjectionMatrices;
    }

private:
    uint32_t m_nSize = 0u;
    uint32_t m_nRes = 0u;
    GLFramebufferObject m_FBO = GLFramebufferObject(0);
    GLTexture2DArray m_TextureArrays[ChannelCount];
    GLTextureCubeMap m_IndirectionTexture = GLTextureCubeMap(0);
    Vec4f m_FaceViewports[6];
    GLenum m_Attachments[ChannelCount];
    Mat4f m_FaceProjectionMatrices[6];
};

struct GLVirtualCubeMapContainerUniform {
    GLUniform<GLSLCubeSamplerf> indirectionTexture;
    GLUniform<GLSLSampler2DArrayf> texture;

    GLVirtualCubeMapContainerUniform(const GLProgram& program):
        indirectionTexture(program, "uVirtualCubeMapContainer.indirectionTexture"),
        texture(program, "uVirtualCubeMapContainer.texture") {
    }

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void set(uint32_t channelIdx,
             const GLVirtualCubeMapContainer<ChannelCount, HasDepthBuffer>& container) {
        indirectionTexture.set(container.getIndirectionTexture().getTextureHandle());
        texture.set(container.getMapArray(channelIdx).getTextureHandle());
    }
};

}
