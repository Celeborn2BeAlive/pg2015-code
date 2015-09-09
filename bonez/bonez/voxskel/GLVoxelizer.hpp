#pragma once

#include <vector>

#include <bonez/maths/maths.hpp>
#include <bonez/maths/BBox.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/scene/VoxelGrid.hpp>

#include "CubicalComplex3D.hpp"
#include "GLVoxelFramebuffer.hpp"

namespace BnZ {

    class GLVoxelizer {
    public:
        int m_res;
        int m_numRenderTargets;
        float m_AABCLength;
        float m_voxelLength;
        glm::vec3 m_meshCenter;
        glm::vec3 m_origBBox;
        GLVoxelFramebuffer m_frameBuffer;

        GLuint m_VoxelGridTextureObject;

        GLProgram m_Program;

    public:
        GLVoxelizer(const GLShaderManager& shaderManager);

        ~GLVoxelizer();

        /**
         * @brief voxelize Voxelize a mesh
         * @param resolution The resolution for the voxel grid
         * @param mesh The mesh
         *
         * This function affects the following states of OpenGL:
         * - GL_CURRENT_PROGRAM
         * - GL_VIEWPORT
         * - GL_DEPTH_TEST
         * - GL_CULL_FACE
         * - GL_COLOR_LOGIC_OP
         * - GL_LOGIC_OP_MODE
         */
        void initGLState(int resolution, BBox3f bbox);

        void restoreGLState();

        float getVoxelLength() const {
            return m_voxelLength;
        }

        glm::vec3 getOrigBBox() const {
            return m_origBBox;
        }

        glm::mat4 getGridToWorldMatrix() const {
            return glm::scale(glm::translate(glm::mat4(1.f), m_origBBox), glm::vec3(m_voxelLength));
        }

    };

}
