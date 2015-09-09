#pragma once

#include <bonez/common.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/scene/Ray.hpp>
#include <bonez/viewer/GUI.hpp>
#include <bonez/scene/Intersection.hpp>

#include <bonez/scene/sensors/Sensor.hpp>

namespace BnZ {

struct SurfacePoint;
class Scene;

class Camera: public Sensor {
public:
    virtual ~Camera() {
    }

    Camera() = default;

    Camera(const Mat4f& viewMatrix):
        m_ViewMatrix(viewMatrix), m_RcpViewMatrix(inverse(viewMatrix)) {
    }

    Camera(const Vec3f& eye, const Vec3f& point, const Vec3f& up):
        Camera(lookAt(eye, point, up)) {
    }

    Vec3f getOrigin() const {
        return Vec3f(m_RcpViewMatrix[3]);
    }

    Vec3f getFrontVector() const {
        return -Vec3f(m_RcpViewMatrix[2]);
    }

    Vec3f getLeftVector() const {
        return -Vec3f(m_RcpViewMatrix[0]);
    }

    Vec3f getUpVector() const {
        return Vec3f(m_RcpViewMatrix[1]);
    }

    const Mat4f& getViewMatrix() const {
        return m_ViewMatrix;
    }

    const Mat4f& getRcpViewMatrix() const {
        return m_RcpViewMatrix;
    }

    // Sample an exitant ray
    virtual Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& positionSample,
            const Vec2f& directionSample,
            RaySample& raySample,
            float& positionPdf,
            float& directionPdf,
            Intersection& I,
            float& sampledPointToIncidentDirectionJacobian,
            float& intersectionPdfWrtArea) const {
        raySample.pdf = 0.f;
        return zero<Vec3f>();
    }

    // Sample an exitant ray
    virtual Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf) const override {
        raySample.pdf = 0.f;
        return zero<Vec3f>();
    }

    // Sample the direct importance on point
    // Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
    // Compute the imageSample associated to lensSample that gived the exitant ray reaching point
    virtual Vec3f sampleDirectImportance(const Scene& scene,
                                         const Vec2f& lensSample,
                                         const SurfacePoint& point,
                                         RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
                                         float* sampledPointToIncidentDirectionJacobian = nullptr,
                                         float* pdfWrtArea = nullptr, // Return the pdf of sampling this point, wrt area
                                         float* revPdfWrtSolidAngle = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
                                         float* revPdfWrtArea = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt area
                                         Vec2f* imageSample = nullptr) const {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }

    virtual GUI::WindowScope exposeIO(GUI& gui) {
        auto window = gui.addWindow("Camera");
        if(window) {
            gui.addValue("position", m_RcpViewMatrix[3]);
            gui.addValue("right", m_RcpViewMatrix[0]);
            gui.addValue("up", m_RcpViewMatrix[1]);
            gui.addValue("back", m_RcpViewMatrix[2]);
        }
        return window;
    }

//    bool hasOriginPoint() const {
//        return m_bHasOriginPoint;
//    }

//    const SurfacePoint& getOriginPoint() const {
//        return m_OriginPoint;
//    }

private:
    Mat4f m_ViewMatrix = Mat4f(1.f);
    Mat4f m_RcpViewMatrix = Mat4f(1.f);

//    bool m_bHasOriginPoint = false;
//    SurfacePoint m_OriginPoint; // Optional origin point, allow to generate rays starting from a surface to avoid self-intersection
};

}
