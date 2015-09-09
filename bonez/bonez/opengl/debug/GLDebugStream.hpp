#pragma once

#include <bonez/types.hpp>
#include <bonez/opengl/GLScreenFramebuffer.hpp>

#include "GLDebugObject.hpp"

namespace BnZ {

struct GLDebugStreamData {
    struct LineInstance {
        float lineWidth;
        Vec4u objectID;

        LineInstance(float lineWidth, const Vec4u& objectID):
            lineWidth(lineWidth), objectID(objectID) {
        }
    };

    struct ColorVertex {
        Vec3f position;
        Vec3f color;
        ColorVertex(const Vec3f& pos, const Vec3f& col):
            position(pos), color(col) {
        }
    };

    std::vector<GLDebugObjectInstance> m_SphereInstances;
    std::vector<GLDebugObjectInstance> m_ArrowInstances;

    std::vector<ColorVertex> m_LineVertices;
    std::vector<LineInstance> m_LineInstances;

    std::vector<ColorVertex> m_PointVertices;

    void clearObjects() {
        m_SphereInstances.clear();
        m_ArrowInstances.clear();
        m_LineVertices.clear();
        m_LineInstances.clear();
        m_PointVertices.clear();
    }

    void addSphere(const Vec3f& center, float radius,
                   const Vec3f& color, const Vec4u& objectID = GLScreenFramebuffer::NULL_OBJECT_ID);

    void addArrow(const Vec3f& position, const Vec3f& direction, float length, float baseRadius,
                  const Vec3f& color, const Vec4u& objectID = GLScreenFramebuffer::NULL_OBJECT_ID);

    void addLine(const Vec3f& org, const Vec3f& dst,
                  const Vec3f& orgColor, const Vec3f& dstColor, float lineWidth,
                  const Vec4u& objectID = GLScreenFramebuffer::NULL_OBJECT_ID);

    void addPoints(uint32_t count, const Vec3f* points, const Vec3f* colors,
                   const Vec4u& objectID = GLScreenFramebuffer::NULL_OBJECT_ID);

    void addBBox(const Vec3f& upper, const Vec3f& lower,
                 const Vec3f& color, float lineWidth, const Vec4u& objectID = GLScreenFramebuffer::NULL_OBJECT_ID);
};

}
