#pragma once

#include <bonez/types.hpp>
#include <bonez/scene/sensors/ProjectiveCamera.hpp>
#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLGBuffer.hpp>

#include "GLDebugObject.hpp"
#include "GLDebugStream.hpp"

namespace BnZ {

class GLDebugRenderer {
public:
    GLDebugRenderer(const GLShaderManager& shaderManager);

    void render(const Mat4f& viewProjMatrix, const Mat4f& viewMatrix);

    void render(const ProjectiveCamera& camera);

    void addStream(GLDebugStreamData* pStream) {
        m_Streams.emplace_back(pStream);
    }

    void clearStreams() {
        m_Streams.clear();
    }

    float getArrowBase() const {
        return m_fArrowBase;
    }

    float getArrowLength() const {
        return m_fArrowLength;
    }

    void setArrowSize(float arrowBase, float arrowLength) {
        m_fArrowBase = arrowBase;
        m_fArrowLength = arrowLength;
    }

private:
    float m_fArrowBase = 0.f;
    float m_fArrowLength = 0.f;

    GLDebugObject m_Sphere;
    GLDebugObject m_Disk;
    GLDebugObject m_Circle;
    GLDebugObject m_Cone;
    GLDebugObject m_Cylinder;

    GLBuffer<GLDebugObjectInstance> m_InstanceBuffer;

    // To draw lines and points:
    GLBuffer<GLDebugStreamData::ColorVertex> m_VBO;
    GLVertexArray m_VAO;

    GLProgram m_Program;

    std::vector<GLDebugStreamData*> m_Streams;

    struct DrawLinesPass {
        GLProgram m_Program;
        BNZ_GLUNIFORM(m_Program, Mat4f, uMVPMatrix);
        BNZ_GLUNIFORM(m_Program, Vec4u, uObjectID);

        DrawLinesPass(const GLShaderManager& shaderManager);
    };

    DrawLinesPass m_DrawLinesPass;

    struct DrawPointsPass {
        GLProgram m_Program;
        BNZ_GLUNIFORM(m_Program, Mat4f, uMVPMatrix);
        BNZ_GLUNIFORM(m_Program, Vec4u, uObjectID);

        DrawPointsPass(const GLShaderManager& shaderManager);
    };

    DrawPointsPass m_DrawPointsPass;
};

}
