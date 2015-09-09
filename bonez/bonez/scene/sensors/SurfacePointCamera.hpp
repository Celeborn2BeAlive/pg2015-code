#pragma once

#include "Camera.hpp"

namespace BnZ {

class SurfacePointCamera: public Camera {
public:
    SurfacePointCamera() = default;

    SurfacePointCamera(const SurfacePoint& point):
        Camera(BnZ::getViewMatrix(point.P, point.Ns)),
        m_Point(point) {
    }

    // Sample an exitant ray and compute its intersection with the scene
    Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf,
                Intersection& I,
                float& sampledPointToIncidentDirectionJacobian,
                float& intersectionPdfWrtArea) const override {
        auto importance = sampleExitantRay(scene, positionSample, directionSample,
                                           raySample, positionPdf, directionPdf);

        sampledPointToIncidentDirectionJacobian = 0.f;

        I = scene.intersect(raySample.value);
        if(I) {
            intersectionPdfWrtArea = directionPdf * abs(dot(I.Ns, -raySample.value.dir)) / sqr(I.distance);
        } else {
            intersectionPdfWrtArea = 0.f;
        }

        return importance;
    }

    // Sample an exitant ray
    Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf) const override {
        auto localDirection = -paraboloidMapping(directionSample);
        // We look toward negative hemisphere only
        if(localDirection.z > 0.f) {
            raySample.pdf = 0.f;
            return zero<Vec3f>();
        }
        auto ray = Ray(m_Point, Vec3f(getRcpViewMatrix() * Vec4f(localDirection, 0.f)));

        positionPdf = 1.f;
        directionPdf = 1.f / paraboloidMappingJacobian(directionSample);

        raySample = RaySample(ray, positionPdf * directionPdf);

        return Vec3f(raySample.pdf);
    }

    // Sample the direct importance on point
    // Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
    // Compute the imageSample associated to lensSample that gived the exitant ray reaching point
    Vec3f sampleDirectImportance(
            const Scene& scene,
            const Vec2f& positionSample,
            const SurfacePoint& point,
            RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
            float* sampledPointToIncidentDirectionJacobian = nullptr,
            float* pdfWrtArea = nullptr, // Return the pdf of sampling this point, wrt area
            float* revPdfWrtSolidAngle = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
            float* revPdfWrtArea = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt area
            Vec2f* imageSample = nullptr
    ) const override {
        auto directionToPoint = point.P - getOrigin();
        auto distanceToPoint = length(directionToPoint);
        directionToPoint /= distanceToPoint;

        auto cosAtCamera = dot(getFrontVector(), directionToPoint);

        if(cosAtCamera <= 0.f) {
            shadowRay.pdf = 0.f;
            return zero<Vec3f>();
        }

        auto sqrDistToPoint = sqr(distanceToPoint);
        auto pdf = sqrDistToPoint;

        shadowRay = RaySample(
                        Ray(point, m_Point, -directionToPoint, distanceToPoint), pdf);

        if(sampledPointToIncidentDirectionJacobian) {
            *sampledPointToIncidentDirectionJacobian = 0.f;
        }

        if(pdfWrtArea) {
            *pdfWrtArea = 1.f;
        }

        auto directionSample = rcpParaboloidMapping(directionToPoint);
        if(imageSample) {
            *imageSample = directionSample;
        }

        auto fRevPdfWrtSolidAngle =  1.f / paraboloidMappingJacobian(directionSample);

        if(revPdfWrtSolidAngle) {
            *revPdfWrtSolidAngle = fRevPdfWrtSolidAngle;
        }

        if(revPdfWrtArea) {
            auto cosToCamera = abs(dot(-directionToPoint, point.Ns));
            *revPdfWrtArea = sqrDistToPoint == 0.f ? 0.f : fRevPdfWrtSolidAngle * (cosToCamera / sqrDistToPoint);
        }

        return Vec3f(fRevPdfWrtSolidAngle);
    }

    const SurfacePoint& getSurfacePoint() const {
        return m_Point;
    }

private:
    SurfacePoint m_Point;
};

}
