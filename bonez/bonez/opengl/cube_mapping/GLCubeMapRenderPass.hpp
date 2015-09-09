#pragma once

#include <bonez/common.hpp>
#include "GLCubeMapContainer.hpp"
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLScene.hpp>
#include <bonez/utils/CubeMapUtils.hpp>

#include "GLCubeMappingMVPMatrixBufferComputePass.hpp"

namespace BnZ {

class GLCubeMapRenderPass {
public:
    GLCubeMapRenderPass(const GLShaderManager& shaderManager,
        const std::vector<std::string>& shaders =
            std::vector<std::string>(1, "cube_mapping/cubeMapping.fs")) :
        m_RenderPass(shaderManager, shaders),
        m_DrawPass(shaderManager),
        m_ConvertPass(shaderManager),
        m_ConvertPass2(shaderManager),
        m_DualParaboloidConversionPass(shaderManager),
        m_MVPMatrixBufferComputePass(shaderManager) {
    }

    // viewMatrixBuffer must be the GPU address of a resident readable buffer
    template<uint32_t ChannelCount>
    void render(const GLScene& scene,
                GLCubeMapContainer<ChannelCount, true>& container,
                const GLBufferStorage<Mat4f>& viewMatrixBuffer,
                uint32_t count,
                float zNear, float zFar) {
        container.computeFaceProjMatrices(zNear, zFar);
        auto pFaceProjMatrix = container.getFaceProjMatrices();

        auto MVPMatrixBuffer = genBufferStorage<Mat4f>(6 * count, nullptr, GL_MAP_WRITE_BIT);
        m_MVPMatrixBufferComputePass.compute(pFaceProjMatrix,
                                             count,
                                             viewMatrixBuffer,
                                             MVPMatrixBuffer);

        glEnable(GL_DEPTH_TEST);

        m_RenderPass.m_Program.use();

        glViewport(0, 0, container.getResolution(), container.getResolution());

        container.bindForDrawing();
        container.setDrawBuffers();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        viewMatrixBuffer.bindBase(GL_SHADER_STORAGE_BUFFER, 0);
        MVPMatrixBuffer.bindBase(GL_SHADER_STORAGE_BUFFER, 1);

        scene.render(m_RenderPass.m_MaterialUniforms, count);
    }

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void drawMap(const GLCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
                 uint32_t index,
                 uint32_t channelIndex,
                 float zNear, float zFar,
                 const GLScreenTriangle& triangle) {
        glDisable(GL_DEPTH_TEST);

        m_DrawPass.m_Program.use();

        m_DrawPass.uZNear.set(zNear);
        m_DrawPass.uZFar.set(zFar);
        m_DrawPass.uMapIndex.set(GLfloat(index));
        m_DrawPass.uDrawDepth.set(HasDepthBuffer && channelIndex == ChannelCount - 1);
        m_DrawPass.uCubeMapContainer.set(channelIndex, 0u, container);

        triangle.render();
    }

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void convertToSphericalMaps(
            const GLCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
            const GLTexture2DArray* sphereMapArrays) {
        m_ConvertPass.m_Program.use();

        Vec3u groupSize(16, 16, 4);

        for(auto i = 0u; i < ChannelCount; ++i) {
            sphereMapArrays[i].bindImage(0u, 0, GL_WRITE_ONLY, GL_RGBA32F);

            m_ConvertPass.uSphereMap.set(0u);
            m_ConvertPass.uCubeMapContainer.set(i, 0u, container);

            glDispatchCompute(1 + sphereMapArrays[i].getWidth() / groupSize.x,
                              1 + sphereMapArrays[i].getHeight() / groupSize.y,
                              1 + container.size() / groupSize.z);
        }

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void convertToSphericalMaps(
            const GLCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
            const GLBufferStorage<Vec4f>* sphereMapBuffers,
            uint32_t sphereMapWidth, uint32_t sphereMapHeight) {
        m_ConvertPass2.m_Program.use();

        Vec3u groupSize(16, 16, 4);

        for(auto i = 0u; i < ChannelCount; ++i) {
            sphereMapBuffers[i].bindBase(GL_SHADER_STORAGE_BUFFER, 0);
            m_ConvertPass2.uCubeMapContainer.set(i, 0u, container);
            m_ConvertPass2.uSphereMapWidth.set(sphereMapWidth);
            m_ConvertPass2.uSphereMapHeight.set(sphereMapHeight);
            m_ConvertPass2.uSphereMapCount.set(container.size());

            glDispatchCompute(1 + sphereMapWidth / groupSize.x,
                              1 + sphereMapHeight / groupSize.y,
                              1 + container.size() / groupSize.z);
        }

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    template<uint32_t ChannelCount, bool HasDepthBuffer>
    void convertToDualParaboloidMaps(
            const GLCubeMapContainer<ChannelCount, HasDepthBuffer>& container,
            const GLBufferStorage<Vec4f>* dualParaboloidMapBuffers,
            uint32_t paraboloidMapWidth, uint32_t paraboloidMapHeight) {
        m_DualParaboloidConversionPass.m_Program.use();

        Vec3u groupSize(16, 16, 4);

        for(auto i = 0u; i < ChannelCount; ++i) {
            dualParaboloidMapBuffers[i].bindBase(GL_SHADER_STORAGE_BUFFER, 0);
            m_DualParaboloidConversionPass.uCubeMapContainer.set(i, 0u, container);
            m_DualParaboloidConversionPass.uParaboloidMapWidth.set(paraboloidMapWidth);
            m_DualParaboloidConversionPass.uParaboloidMapHeight.set(paraboloidMapHeight);
            m_DualParaboloidConversionPass.uDualParaboloidMapCount.set(container.size());

            glDispatchCompute(1 + 2 * paraboloidMapWidth / groupSize.x,
                              1 + paraboloidMapHeight / groupSize.y,
                              1 + container.size() / groupSize.z);
        }

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

private:
    struct RenderPass {
        GLProgram m_Program;

        GLMaterialUniforms m_MaterialUniforms { m_Program };

        RenderPass(const GLShaderManager& shaderManager,
                   const std::vector<std::string>& shaders):
            m_Program(shaderManager.buildProgram(
                        concat(std::vector<std::string>({
                                  "cube_mapping/cubeMapping.vs",
                                  "cube_mapping/cubeMapping.gs"}),
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

        GLCubeMapContainerUniform uCubeMapContainer { m_Program };

        DrawPass(const GLShaderManager& shaderManager):
            m_Program(shaderManager.buildProgram({
                "utils.fs",
                "image.vs",
                "cube_mapping/cubeMapUtils.fs",
                getCubeMapSphereTextureShader(".fs"),
                "cube_mapping/drawCubeMap.fs"
            })) {
        }
    };

    DrawPass m_DrawPass;

    struct ConvertPass {
        GLProgram m_Program;

        BNZ_GLUNIFORM(m_Program, GLSLImage2DArrayf, uSphereMap);
        GLCubeMapContainerUniform uCubeMapContainer { m_Program };

        ConvertPass(const GLShaderManager& shaderManager) :
            m_Program(shaderManager.buildProgram({
                "cube_mapping/cubeMapUtils.cs",
                getCubeMapSphereTextureShader(".cs"),
                "cube_mapping/cubeMapsToSphericalMaps.cs"
            })) {
        }
    };

    ConvertPass m_ConvertPass;

    struct ConvertPass2 {
        GLProgram m_Program;

        GLCubeMapContainerUniform uCubeMapContainer { m_Program };
        BNZ_GLUNIFORM(m_Program, GLuint, uSphereMapWidth);
        BNZ_GLUNIFORM(m_Program, GLuint, uSphereMapHeight);
        BNZ_GLUNIFORM(m_Program, GLuint, uSphereMapCount);

        ConvertPass2(const GLShaderManager& shaderManager) :
            m_Program(shaderManager.buildProgram({
                "cube_mapping/cubeMapUtils.cs",
                getCubeMapSphereTextureShader(".cs"),
                "cube_mapping/cubeMapsToSphericalMapBuffers.cs"
            })) {
        }
    };

    ConvertPass2 m_ConvertPass2;

    struct DualParaboloidConversion {
        GLProgram m_Program;

        GLCubeMapContainerUniform uCubeMapContainer { m_Program };
        BNZ_GLUNIFORM(m_Program, GLuint, uParaboloidMapWidth);
        BNZ_GLUNIFORM(m_Program, GLuint, uParaboloidMapHeight);
        BNZ_GLUNIFORM(m_Program, GLuint, uDualParaboloidMapCount);

        DualParaboloidConversion(const GLShaderManager& shaderManager) :
            m_Program(shaderManager.buildProgram({
                "cube_mapping/cubeMapUtils.cs",
                getCubeMapSphereTextureShader(".cs"),
                "cube_mapping/cubeMapsToDualParaboloidMapBuffers.cs"
            })) {
        }
    };

    DualParaboloidConversion m_DualParaboloidConversionPass;

    GLCubeMappingMVPMatrixBufferComputePass m_MVPMatrixBufferComputePass;
};

}
