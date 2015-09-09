#include "GLDebugRenderer.hpp"
#include <bonez/maths/maths.hpp>

namespace BnZ {

GLDebugRenderer::GLDebugRenderer(const GLShaderManager &shaderManager):
    m_Program(shaderManager.buildProgram({ "debug.vs", "debug.fs" })),
    m_DrawLinesPass(shaderManager),
    m_DrawPointsPass(shaderManager) {
    buildSphere(16, 8, m_Sphere);
    buildDisk(8, m_Disk);
    buildCone(8, 1, m_Cone);
    buildCylinder(8, 1, m_Cylinder);
    buildCircle(32, m_Circle);

    for(auto object: { &m_Sphere, &m_Disk, &m_Cone, &m_Cylinder, &m_Circle }) {
        object->setInstanceBuffer(m_InstanceBuffer.glId());
    }

    m_VAO.enableVertexAttrib(0);
    m_VAO.enableVertexAttrib(1);
    m_VAO.vertexAttribOffset(m_VBO.glId(), 0, 3, GL_FLOAT, GL_FALSE,
                                  sizeof(GLDebugStreamData::ColorVertex), BNZ_OFFSETOF(GLDebugStreamData::ColorVertex, position));
    m_VAO.vertexAttribOffset(m_VBO.glId(), 1, 3, GL_FLOAT, GL_FALSE,
                                  sizeof(GLDebugStreamData::ColorVertex), BNZ_OFFSETOF(GLDebugStreamData::ColorVertex, color));
}

GLDebugRenderer::DrawLinesPass::DrawLinesPass(const GLShaderManager &shaderManager):
    m_Program(shaderManager.buildProgram({ "debugLines.vs", "debugLines.fs" })) {
}

GLDebugRenderer::DrawPointsPass::DrawPointsPass(const GLShaderManager &shaderManager):
    m_Program(shaderManager.buildProgram({ "debugDrawPoints.vs", "debugDrawPoints.fs" })) {
}

void GLDebugRenderer::render(const Mat4f& vpMatrix, const Mat4f& vMatrix) {
    glEnable(GL_DEPTH_TEST);

    m_Program.use();

    for(auto pStream: m_Streams) {
        std::vector<GLDebugObjectInstance> copy;
        copy = pStream->m_ArrowInstances;
        for (auto& inst : copy) {
            inst.mvpMatrix = translate(scale(vpMatrix * inst.mvpMatrix, Vec3f(3.f, 1.f, 3.f)), Vec3f(0, 1, 0));
            inst.mvMatrix = translate(scale(vpMatrix * inst.mvpMatrix, Vec3f(3.f, 1.f, 3.f)), Vec3f(0, 1, 0));
        }

        m_InstanceBuffer.setData(copy, GL_STREAM_DRAW);
        m_Cone.draw(m_InstanceBuffer.size());

        copy = pStream->m_SphereInstances;
        for(auto& inst: copy) {
            inst.mvpMatrix = vpMatrix * inst.mvpMatrix;
            inst.mvMatrix = vMatrix * inst.mvMatrix;
        }

        m_InstanceBuffer.setData(copy, GL_STREAM_DRAW);
        m_Sphere.draw(m_InstanceBuffer.size());

        copy = pStream->m_ArrowInstances;
        for (auto& inst : copy) {
            inst.mvpMatrix = vpMatrix * inst.mvpMatrix;
            inst.mvMatrix = vMatrix * inst.mvMatrix;
        }

        m_InstanceBuffer.setData(copy, GL_STREAM_DRAW);
        m_Cylinder.draw(m_InstanceBuffer.size());
    }

    m_DrawLinesPass.m_Program.use();

    m_DrawLinesPass.uMVPMatrix.set(vpMatrix);

    m_VAO.bind();

    for(auto pStream: m_Streams) {
        m_VBO.setData(pStream->m_LineVertices, GL_STREAM_DRAW);

        uint32_t offset = 0;

        for(const auto& instance:pStream->m_LineInstances) {
            glLineWidth(instance.lineWidth);
            m_DrawLinesPass.uObjectID.set(instance.objectID);
            glDrawArrays(GL_LINES, offset, 2);
            offset += 2;
        }
    }

    m_DrawPointsPass.m_Program.use();

    m_DrawPointsPass.uMVPMatrix.set(vpMatrix);

    glEnable(GL_PROGRAM_POINT_SIZE);

    for(auto pStream: m_Streams) {
        m_VBO.setData(pStream->m_PointVertices, GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, pStream->m_PointVertices.size());
    }

    glDisable(GL_PROGRAM_POINT_SIZE);
}

void GLDebugRenderer::render(const ProjectiveCamera& camera) {
    render(camera.getViewProjMatrix(), camera.getViewMatrix());
}

}
