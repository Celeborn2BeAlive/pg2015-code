#include "PointLight.hpp"
#include <bonez/scene/Scene.hpp>

namespace BnZ {

PointLight::PointLight(const Vec3f& position,
           const Vec3f& intensity):
    m_fIntensity(length(intensity)),
    m_Color(m_fIntensity ? intensity / m_fIntensity : intensity),
    m_Position(position),
    m_Intensity(intensity) {
}

Vec3f PointLight::getPowerUpperBound(const Scene& scene) const {
    return four_pi<float>() * Vec3f(m_Intensity);
}

void PointLight::exposeIO(GUI& gui, UpdateFlag& flag) {
    if (gui.addVarRW(BNZ_GUI_VAR(m_Position))) {
        flag.addUpdate();
    }

    if (gui.addVarRW(BNZ_GUI_VAR(m_Color)) || gui.addVarRW(BNZ_GUI_VAR(m_fIntensity))) {
        m_Intensity = m_fIntensity * m_Color;
        flag.addUpdate();
    }
}

void PointLight::accept(LightVisitor& visitor) {
    visitor.visit(*this);
}

// Sample an exitant ray
Vec3f PointLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf) const
{
    auto dirSample = uniformSampleSphere(raySample.x, raySample.y);

    emissionVertexPdf = 1.f;
    auto directionPdf = dirSample.pdf;

    auto pdf = directionPdf;

    exitantRaySample = RaySample(Ray(m_Position, dirSample.value), pdf);

    return m_Intensity;
}

Vec3f PointLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf,
        Intersection& intersection,
        float& emissionVertexIncidentDirectionJacobian,
        float& intersectionPdfWrtArea) const
{
    auto dirSample = uniformSampleSphere(raySample.x, raySample.y);

    emissionVertexPdf = 1.f;
    auto directionPdf = dirSample.pdf;

    auto pdf = directionPdf;

    exitantRaySample = RaySample(Ray(m_Position, dirSample.value), pdf);

    intersection = scene.intersect(exitantRaySample.value);
    if (intersection) {
        emissionVertexIncidentDirectionJacobian = 0.f;
        intersectionPdfWrtArea = directionPdf * abs(dot(-exitantRaySample.value.dir, intersection.Ns)) / sqr(intersection.distance);
    }
    else {
        emissionVertexIncidentDirectionJacobian = 0;
        intersectionPdfWrtArea = 0;
    }

    return m_Intensity;
}

// Sample the direct illumination on point
// Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
// Compute the imageSample associated to lensSample that gived the exitant ray reaching point
Vec3f PointLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const SurfacePoint& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf,
        float& pointPdfWrtArea
) const {
    auto wo = point.P - m_Position;
    auto dist = length(wo);
    if (dist == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }
    wo /= dist;

    shadowRay = RaySample(Ray(point, -wo, dist), sqr(dist));

    emissionVertexToIncidentDirectionJacobian = 0.f;

    emissionVertexPdf = 1.f;

    pointPdfWrtArea = uniformSampleSpherePDF() * abs(dot(point.Ns, wo)) /
        sqr(dist);

    return m_Intensity;
}

Vec3f PointLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec3f& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf
) const {
    auto wo = point - m_Position;
    auto dist = length(wo);
    if (dist == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }
    wo /= dist;

    shadowRay = RaySample(Ray(point, -wo, dist), sqr(dist));

    emissionVertexToIncidentDirectionJacobian = 0.f;

    emissionVertexPdf = 1.f;

    return m_Intensity;
}

Vec3f PointLight::evalBoundedDirectIllumination(
            const Scene& scene,
            const Vec2f& positionSample,
            const SurfacePoint& point,
            float radius,
            Ray& shadowRay
) const {
    auto wo = point.P - m_Position;
    auto dist = length(wo);
    if (dist == 0.f) {
        return zero<Vec3f>();
    }
    wo /= dist;

    shadowRay = Ray(point, -wo, dist);

    auto distMin = max(0.f, dist - radius);
    if(distMin == 0.f) {
        return zero<Vec3f>();
    }

    return m_Intensity / sqr(distMin);
}

void PointLight::sampleVPL(
        const Scene& scene,
        float lightPdf,
        const Vec2f& emissionVertexSample,
        EmissionVPLContainer& container) const {
    container.add(PointLightVPL {
                      m_Position,
                      m_Intensity / lightPdf
                  });
}

}
