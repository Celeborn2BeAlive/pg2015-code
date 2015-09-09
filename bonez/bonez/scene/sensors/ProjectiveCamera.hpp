#pragma once

#include <bonez/common.hpp>
#include "Camera.hpp"
#include <bonez/scene/Ray.hpp>

namespace BnZ {

class ProjectiveCamera: public Camera {
public:
    ProjectiveCamera() = default;

    ProjectiveCamera(const Mat4f& viewMatrix,
                     float fovy, float resX, float resY,
                     float zNear, float zFar);

    const Mat4f& getProjMatrix() const {
        return m_ProjMatrix;
    }

    const Mat4f& getRcpProjMatrix() const {
        return m_RcpProjMatrix;
    }

    Mat4f getViewProjMatrix() const {
        return m_ViewProjMatrix;
    }

    Mat4f getRcpViewProjMatrix() const {
        return m_RcpViewProjMatrix;
    }

    float getFovY() const {
        return m_fFovY;
    }

//    Ray doGetRay(const Vec2f& ndcPosition) const override;

//    bool getNDC(const Vec3f& P, Vec2f& ndc) const override;

    float getZFar() const {
        return m_fZFar;
    }

    float getFocalDistance() const {
        return m_fDistanceToImagePlane;
    }

//    float imageToSurfaceJacobian(const SurfacePoint& point) const override;

//    float imageToNdcJacobian(const Vec2f& ndc) const override;

//    float ndcToRayJacobian(const Vec2f& ndc) const override;

    virtual Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& positionSample,
            const Vec2f& directionSample,
            RaySample& raySample,
            float& positionPdf,
            float& directionPdf,
            Intersection& I,
            float& sampledPointToIncidentDirectionJacobian,
            float& intersectionPdfWrtArea) const override;

    // Sample an exitant ray
    virtual Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf) const override;

    virtual Vec3f sampleDirectImportance(const Scene& scene,
                                         const Vec2f& lensSample,
                                         const SurfacePoint& point,
                                         RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
                                         float* sampledPointToIncidentDirectionJacobian = nullptr,
                                         float* pdfWrtArea = nullptr, // Return the pdf of sampling this point, wrt area
                                         float* revPdfWrtSolidAngle = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
                                         float* revPdfWrtArea = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt area
                                         Vec2f* imageSample = nullptr) const override;

    GUI::WindowScope exposeIO(GUI& gui) override {
        auto window = Camera::exposeIO(gui);
        if(window) {
            gui.addValue(BNZ_GUI_VAR(m_fDistanceToImagePlane));

            auto p1 = m_RcpProjMatrix * Vec4f(-1, -1, -1.f, 1.f);
            p1 /= p1.w;
            auto p2 = m_RcpProjMatrix * Vec4f(1, -1, -1.f, 1.f);
            p2 /= p2.w;
            auto p3 = m_RcpProjMatrix * Vec4f(1, 1, -1.f, 1.f);
            p3 /= p3.w;
            auto p4 = m_RcpProjMatrix * Vec4f(-1, 1, -1.f, 1.f);
            p4 /= p4.w;

            gui.addValue(BNZ_GUI_VAR(p1));

            auto v1 = p2 - p1;
            auto v2 = p4 - p1;
            auto area = length(cross(Vec3f(v1), Vec3f(v2)));
            gui.addValue(BNZ_GUI_VAR(area));

            gui.addValue(BNZ_GUI_VAR(m_fZNear));
            gui.addValue(BNZ_GUI_VAR(m_fZFar));

            auto H = 2 * m_fZNear * tan(0.5f * m_fFovY);
            auto W = H * m_fResX / m_fResY;

            gui.addValue(BNZ_GUI_VAR(W));
            gui.addValue(BNZ_GUI_VAR(H));
            gui.addValue(BNZ_GUI_VAR(W * H));
        }
        return window;
    }

private:
    Mat4f m_ProjMatrix = Mat4f(1.f);
    Mat4f m_RcpProjMatrix = Mat4f(1.f);
    float m_fFovY = 0.f;
    float m_fResX = 0.f, m_fResY = 0.f;
    float m_fZNear = 0.f;
    float m_fZFar = 0.f;
    float m_fDistanceToImagePlane = 0.f;
    Mat4f m_ViewProjMatrix = getProjMatrix() * getViewMatrix();
    Mat4f m_RcpViewProjMatrix = inverse(m_ViewProjMatrix);
    float m_fNDCToImageFactor = 0.f;
};

}
