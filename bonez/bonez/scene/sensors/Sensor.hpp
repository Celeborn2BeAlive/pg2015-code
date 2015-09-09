#pragma once

#include <bonez/scene/Ray.hpp>
#include <bonez/scene/SurfacePoint.hpp>
#include <bonez/scene/Intersection.hpp>

namespace BnZ {

class Scene;

class Sensor {
public:
    virtual ~Sensor() {
    }

    // Sample an exitant ray and compute its intersection with the scene
    virtual Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf,
                Intersection& I,
                float& sampledPointToIncidentDirectionJacobian,
                float& intersectionPdfWrtArea) const = 0;

    // Sample an exitant ray
    virtual Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& positionSample,
                const Vec2f& directionSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf) const = 0;

    // Sample the direct importance on point
    // Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
    // Compute the imageSample associated to lensSample that gived the exitant ray reaching point
    virtual Vec3f sampleDirectImportance(
            const Scene& scene,
            const Vec2f& positionSample,
            const SurfacePoint& point,
            RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
            float* sampledPointToIncidentDirectionJacobian = nullptr,
            float* pdfWrtArea = nullptr, // Return the pdf of sampling this point, wrt area
            float* revPdfWrtSolidAngle = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
            float* revPdfWrtArea = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt area
            Vec2f* imageSample = nullptr
    ) const = 0;

private:
};

inline Vec3f sampleExitantRay(
        const Sensor& sensor,
        const Scene& scene,
        const Vec2f& positionSample,
        const Vec2f& directionSample,
        RaySample& raySample,
        Intersection& I) {
    float positionPdf, directionPdf, sampledPointToIncidentDirectionJacobian,
            intersectionPdfWrtArea;
    return sensor.sampleExitantRay(scene, positionSample, directionSample, raySample,
                            positionPdf, directionPdf, I, sampledPointToIncidentDirectionJacobian,
                            intersectionPdfWrtArea);
}

inline Intersection tracePrimaryRay(
        const Sensor& sensor,
        const Scene& scene,
        const Vec2f& positionSample,
        const Vec2f& directionSample,
        RaySample& raySample) {
    Intersection I;
    sampleExitantRay(sensor, scene, positionSample, directionSample, raySample, I);
    return I;
}

inline Intersection tracePrimaryRay(
        const Sensor& sensor,
        const Scene& scene,
        const Vec2f& positionSample,
        const Vec2f& directionSample) {
    RaySample raySample;
    return tracePrimaryRay(sensor, scene, positionSample, directionSample, raySample);
}

}
