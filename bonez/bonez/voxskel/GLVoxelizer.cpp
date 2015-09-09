#include <iostream>
#include "GLVoxelizer.hpp"
#include "VoxelBuffer.hpp"

namespace BnZ {

    GLVoxelizer::GLVoxelizer(const GLShaderManager& shaderManager):
        m_Program(shaderManager.buildProgram({ "voxskel/Voxskel.vs", "voxskel/Voxskel.gs", "voxskel/Voxskel.fs" })) {
        glGenTextures(1, &m_VoxelGridTextureObject);
    }

    GLVoxelizer::~GLVoxelizer() {
        glDeleteTextures(1, &m_VoxelGridTextureObject);
    }

    static GLint getUniformLocation(GLuint program, const char* name) {
        GLint uniformLocation = glGetUniformLocation(program, name);
        if (uniformLocation < 0) {
            std::cerr << name << " not found in program " << program << std::endl;
        }
        return uniformLocation;
    }

    void GLVoxelizer::initGLState(int res, BBox3f bbox) {
        auto v = bbox.upper - bbox.lower;

        auto bboxCenter = 0.5f * (bbox.lower + bbox.upper);
        // Increase of 5% the size of the bounding box to ensure that we don't miss any triangle that are at the edge
        bbox.lower = bboxCenter - 0.55f * v;
        bbox.upper = bboxCenter + 0.55f * v;

        m_res = res;
        // Requiered number of color buffer
        m_numRenderTargets = ceil((double)m_res / 128.0);

        auto bboxSize = bbox.size();
        m_AABCLength = reduceMax(bboxSize);

        m_voxelLength = m_AABCLength / (float)m_res;


        m_origBBox = bboxCenter - glm::vec3(0.5f * m_AABCLength);

        // Init 3D Voxel texture
        glBindTexture(GL_TEXTURE_3D, m_VoxelGridTextureObject);

        std::vector<int32_t> zero(res * res * res, 0);

        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32I, res, res, res, 0,
            GL_RED_INTEGER, GL_INT, zero.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_3D, 0);

        glBindImageTexture(0, m_VoxelGridTextureObject, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);

        auto programID = m_Program.glId();
        // Use the shaders
        m_Program.use();

        // Desactivate depth, cullface
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        // Blending activated
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_OR);

        // Init FrameBuffer
        if (!m_frameBuffer.init(m_res, m_res, m_res, m_numRenderTargets)){
            std::cerr << "FBO Error" << std::endl;
        }

        //Projection matrix : Adapt the viewport to the size of the mesh
        glm::mat4 P = glm::ortho(-m_AABCLength * 0.5f,
            m_AABCLength * 0.5f,
            -m_AABCLength * 0.5f,
            m_AABCLength * 0.5f,
            0.f,
            m_AABCLength);

        glm::vec3 position(bboxCenter.x, bboxCenter.y, bboxCenter.z + 0.5 * m_AABCLength);
        glm::vec3 point(bboxCenter.x, bboxCenter.y, bboxCenter.z);
        glm::mat4 V = glm::lookAt(position, point, glm::vec3(0, 1, 0));

        // Get the MVP Matrix
        glm::mat4 MVP = P * V;

        // Geometry shader uniform
        GLint MVPLocation = getUniformLocation(programID, "MVP");
        glUniformMatrix4fv(MVPLocation, 1, GL_FALSE, glm::value_ptr(MVP));

        GLint halfPixelSizeLocation = getUniformLocation(programID, "halfPixelSize");
        glUniform2fv(halfPixelSizeLocation, 1, glm::value_ptr(glm::vec2(1.f / m_res))); //Value given in the thesis

        //GLint halfVoxelSizeNormalizedLocation = getUniformLocation(programID, "halfVoxelSizeNormalized");
        //glUniform1f(halfVoxelSizeNormalizedLocation, 1.f / (2.f * m_res) ); //Value given in the thesis

        GLint numVoxelsLocation = getUniformLocation(programID, "numVoxels");
        glUniform1i(numVoxelsLocation, m_res);

        // Fragment shader uniforms
        GLint origBBoxLocation = getUniformLocation(programID, "origBBox");

        glUniform3fv(origBBoxLocation, 1, glm::value_ptr(m_origBBox));

        GLint numRenderTargetsLocation = getUniformLocation(programID, "numRenderTargets");
        glUniform1i(numRenderTargetsLocation, m_numRenderTargets);

        GLint voxelSizeLocation = getUniformLocation(programID, "voxelSize");
        glUniform1f(voxelSizeLocation, m_voxelLength);

        GLint uVoxelGridLocation = m_Program.getUniformLocation("uVoxelGrid");
        glUniform1i(uVoxelGridLocation, 0);

        m_frameBuffer.bind(GL_DRAW_FRAMEBUFFER);

        // Set the list of draw buffers.
        std::vector<GLenum> DrawBuffers(m_numRenderTargets, 0);
        for (int i = 0; i < m_numRenderTargets; ++i){
            DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        glDrawBuffers(m_numRenderTargets, DrawBuffers.data());

        glViewport(0, 0, m_res, m_res);

        // Clear the window
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //space.resolution = res;
        //space.gridToWorld = glm::scale(glm::translate(glm::mat4(1.f), m_origBBox), glm::vec3(m_voxelLength));
        //space.worldToGrid = glm::inverse(space.gridToWorld);
    }

    void GLVoxelizer::restoreGLState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glLogicOp(GL_COPY);
        glDisable(GL_COLOR_LOGIC_OP);
    }
}
