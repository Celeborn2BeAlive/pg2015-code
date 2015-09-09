#include "GLDebugStream.hpp"

#include <bonez/opengl/utils/GLutils.hpp>
#include <bonez/maths/maths.hpp>

namespace BnZ {

void GLDebugStreamData::addSphere(const Vec3f& center, float radius,
                                const Vec3f& color, const Vec4u& objectID) {
    m_SphereInstances.emplace_back(scale(translate(Mat4f(1.f), center), Vec3f(radius)),
                                   color, objectID);
}

void GLDebugStreamData::addArrow(const Vec3f& position, const Vec3f& normal, float length, float baseRadius,
              const Vec3f& color, const Vec4u& objectID) {

    Mat4f modelMatrix = scale(frameY(position, normal), Vec3f(baseRadius, length, baseRadius));

    m_ArrowInstances.emplace_back(modelMatrix, color, objectID);
}

void GLDebugStreamData::addLine(const Vec3f& org, const Vec3f& dst,
             const Vec3f& orgColor, const Vec3f& dstColor, float lineWidth,
             const Vec4u& objectID) {
    m_LineVertices.emplace_back(org, orgColor);
    m_LineVertices.emplace_back(dst, dstColor);
    m_LineInstances.emplace_back(lineWidth, objectID);
}

void GLDebugStreamData::addPoints(uint32_t count, const Vec3f* points, const Vec3f* colors,
                                            const Vec4u& objectID) {
    for(auto i = 0u; i < count; ++i) {
        m_PointVertices.emplace_back(points[i], colors[i]);
    }
}

void GLDebugStreamData::addBBox(const Vec3f& upper, const Vec3f& lower,
             const Vec3f& color, float lineWidth, const Vec4u& objectID) {
    addLine(lower, Vec3f(upper.x, lower.y, lower.z), color, color, lineWidth);
    addLine(lower, Vec3f(lower.x, upper.y, lower.z), color, color, lineWidth);
    addLine(lower, Vec3f(lower.x, lower.y, upper.z), color, color, lineWidth);

    addLine(upper, Vec3f(lower.x, upper.y, upper.z), color, color, lineWidth);
    addLine(upper, Vec3f(upper.x, lower.y, upper.z), color, color, lineWidth);
    addLine(upper, Vec3f(upper.x, upper.y, lower.z), color, color, lineWidth);

    addLine(Vec3f(lower.x, upper.y, lower.z),
                        Vec3f(upper.x, upper.y, lower.z), color, color, lineWidth);
    addLine(Vec3f(lower.x, upper.y, lower.z),
                        Vec3f(lower.x, upper.y, upper.z), color, color, lineWidth);

    addLine(Vec3f(upper.x, lower.y, lower.z),
                        Vec3f(upper.x, lower.y, upper.z), color, color, lineWidth);
    addLine(Vec3f(upper.x, lower.y, lower.z),
                        Vec3f(upper.x, upper.y, lower.z), color, color, lineWidth);

    addLine(Vec3f(lower.x, upper.y, upper.z),
                        Vec3f(lower.x, lower.y, upper.z), color, color, lineWidth);
    addLine(Vec3f(lower.x, lower.y, upper.z),
                        Vec3f(upper.x, lower.y, upper.z), color, color, lineWidth);
}

}
