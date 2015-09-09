#pragma once

#include "GLVirtualCubeMapContainer.hpp"
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLScene.hpp>
#include <bonez/utils/CubeMapUtils.hpp>

#include "GLCubeMappingMVPMatrixBufferComputePass.hpp"

namespace BnZ {

class GLVirtualCubeMapRenderPass {
public:
    GLVirtualCubeMapRenderPass(const GLShaderManager& shaderManager,
        const std::vector<std::string>& shaders = std::vector<std::string>(1, "cube_mapping/cubeMapping.fs")) :
        m_RenderPass(shaderManager, shaders), m_DrawPass(shaderManager),
        m_ConvertPass(shaderManager),
        m_MVPMatrixBufferComputePass(shaderManager) {
    }

    template<uint32_t ChannelCount>
    void render(const GLScene& scene,
                GLVirtualCubeMapContainer<ChannelCount, true>& container,
                GLBufferAddress<Mat4f> viewMatrixBuffer,
                uint32_t count,
                float zNear, float zFar) {
        container.computeFaceProjMatrices(zNear, zFar);
        auto pFaceProjMatrix = container.getFaceProjMatrices();

        auto MVPMatrixBuffer = genBufferStorage<Mat4f>(6 * count, nullptr, GL_MAP_WRITE_BIT, GL_READ_WRITE);

        m_MVPMatrixBufferComputePass.compute(pFaceProjMatrix,
                                             count,
                                             viewMatrixBuffer,
                                             MVPMatrixBuffer.getGPUAddress());

        glEnable(GL_DEPTH_TEST);

        m_RenderPass.m_Program.use();

        glViewportArrayv(0, 6, (const GLfloat*) container.getFaceViewports());

        container.bindForDrawing();
        container.setDrawBuffers();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_RenderPass.uMVPMatrixBuffer.set(MVPMatrixBuffer.getGPUAddress());
        m_RenderPass.uMVMatrixBuffer.set(viewMatrixBuffer);

        scene.render(m_RenderPass.m_MaterialUniforms, count);
    }

    /*
    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void drawMap(const GLVirtualCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
                 uint32_t index,
                 uint32_t channelIndex,
                 const Context& context) {
        auto depthTest = pushGLState<GL_DEPTH_TEST>();
        depthTest.set(false);

        m_DrawPass.m_Program.use();

        m_DrawPass.uZNear.set(context.getZNear());
        m_DrawPass.uZFar.set(context.getZFar());
        m_DrawPass.uMapIndex.set(GLfloat(index));
        m_DrawPass.uDrawDepth.set(HasDepthBuffer && channelIndex == ChannelCount - 1);
        m_DrawPass.uVirtualCubeMapContainer.set(channelIndex, container);

        context.drawScreenTriangle();
    }*/

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void convertToSphericalMaps(
            const GLVirtualCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
            GLImage2DArrayHandle sphereMapImageHandles[ChannelCount],
            uint32_t sphereMapWidth, uint32_t sphereMapHeight) {
        m_ConvertPass.m_Program.use();

        Vec3u groupSize(16, 16, 4);

        for(auto i = 0u; i < ChannelCount; ++i) {
            m_ConvertPass.uSphereMap.set(sphereMapImageHandles[i]);
            m_ConvertPass.uCubeMapContainer.set(i, container);

            glDispatchCompute(1 + sphereMapWidth / groupSize.x,
                              1 + sphereMapHeight / groupSize.y,
                              1 + container.size() / groupSize.z);
        }

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

private:
    struct RenderPass {
        GLProgram m_Program;

        GLMaterialUniforms m_MaterialUniforms { m_Program };
        BNZ_GLUNIFORM(m_Program, GLBufferAddress<Mat4f>, uMVPMatrixBuffer);
        BNZ_GLUNIFORM(m_Program, GLBufferAddress<Mat4f>, uMVMatrixBuffer);

        RenderPass(const GLShaderManager& shaderManager,
                   const std::vector<std::string>& shaders):
            m_Program(shaderManager.buildProgram(
                        concat(std::vector<std::string>({"cube_mapping/cubeMapping.vs",
                                  "cube_mapping/virtualCubeMapping.gs"}),
                                 shaders))) {
        }
    };

    RenderPass m_RenderPass;

    struct DrawPass {
        GLProgram m_Program;

        BNZ_GLUNIFORM(m_Program, float, uZNear);
        BNZ_GLUNIFORM(m_Program, float, uZFar);
        BNZ_GLUNIFORM(m_Program, GLuint, uMapIndex);
        BNZ_GLUNIFORM(m_Program, bool, uDrawDepth);

        GLVirtualCubeMapContainerUniform uVirtualCubeMapContainer { m_Program };

        DrawPass(const GLShaderManager& shaderManager):
            m_Program(shaderManager.buildProgram({
                "utils.fs",
                "image.vs",
                "cube_mapping/cubeMapUtils.fs",
                getVirtualCubeMapSphereTextureShader(".fs"),
                "cube_mapping/drawVirtualCubeMap.fs"
            })) {
        }
    };

    DrawPass m_DrawPass;

    struct ConvertPass {
        GLProgram m_Program;

        BNZ_GLUNIFORM(m_Program, GLImage2DArrayHandle, uSphereMap);
        GLVirtualCubeMapContainerUniform uCubeMapContainer { m_Program };

        ConvertPass(const GLShaderManager& shaderManager) :
            m_Program(shaderManager.buildProgram({
                "cube_mapping/cubeMapUtils.cs",
                getVirtualCubeMapSphereTextureShader(".cs"),
                "cube_mapping/cubeMapsToSphericalMaps.cs"
            })) {
        }
    };

    ConvertPass m_ConvertPass;

    GLCubeMappingMVPMatrixBufferComputePass m_MVPMatrixBufferComputePass;
};

}
