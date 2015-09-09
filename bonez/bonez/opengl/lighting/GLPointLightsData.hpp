#pragma once

#include <bonez/opengl/cube_mapping/GLCubeMapContainer.hpp>
#include <bonez/opengl/cube_mapping/GLCubeMapRenderPass.hpp>
#include <bonez/scene/lights/PointLight.hpp>

namespace BnZ {

template<uint32_t ChannelCount>
class GLPointLightsData {
public:
    struct PointLightData {
        Vec4f m_Position;
        Vec4f m_Intensity;
    };

    GLCubeMapContainer<ChannelCount> m_CubeMapContainer;
    GLBufferStorage<Mat4f> m_MVMatrixBuffer;
    GLBufferStorage<PointLightData> m_PointLightBuffer;
    size_t m_nSize;

    void init(uint32_t cubeMapRes, uint32_t maxSize) {
        if(m_MVMatrixBuffer.size() < maxSize) {
            m_MVMatrixBuffer = genBufferStorage<Mat4f>(maxSize, nullptr, GL_MAP_WRITE_BIT,
                                                       GL_READ_ONLY);
            m_PointLightBuffer = genBufferStorage<PointLightData>(maxSize, nullptr, GL_MAP_WRITE_BIT,
                                                       GL_READ_ONLY);
        }

        if(m_CubeMapContainer.getResolution() != cubeMapRes ||
                m_CubeMapContainer.size() < maxSize) {
            m_CubeMapContainer = GLCubeMapContainer<ChannelCount>(cubeMapRes, maxSize);
        }
    }

    void compute(uint32_t count,
                 const PointLight* pLight,
                 const GLScene& scene,
                 GLCubeMapRenderPass& renderPass,
                 float zNear, float zFar) {
        m_nSize = count;

        auto pMVMatrix = m_MVMatrixBuffer.map(GL_MAP_WRITE_BIT);
        auto pPointLight = m_PointLightBuffer.map(GL_MAP_WRITE_BIT);

        for(auto i = 0u; i < count; ++i) {
            pPointLight->m_Position = Vec4f(pLight->m_Position, 1);
            pPointLight->m_Intensity = Vec4f(pLight->m_Intensity, 0);

            *pMVMatrix++ = getViewMatrix(pLight->m_Position);

            ++pPointLight;
            ++pLight;
        }

        m_MVMatrixBuffer.unmap();
        m_PointLightBuffer.unmap();

        renderPass.render(scene, m_CubeMapContainer, m_MVMatrixBuffer.getGPUAddress(),
                          count, zNear, zFar);
    }

    size_t size() const {
        return m_nSize;
    }
};

}
