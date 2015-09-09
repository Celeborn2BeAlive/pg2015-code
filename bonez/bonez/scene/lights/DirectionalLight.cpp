#include "DirectionalLight.hpp"

#include "bonez/scene/Scene.hpp"

#include <bonez/sampling/shapes.hpp>

namespace BnZ {

static Vec3f computeEmittedRadiance(const Vec3f& exitantPower, const Scene& scene) {
    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);

    return exitantPower / (pi<float>() * sqr(radius));
}

DirectionalLight::DirectionalLight(const Vec3f& wi,
                 const Vec3f& Le):
    m_fLe(length(Le)),
    m_Color(m_fLe ? Le / m_fLe : Le),
    m_IncidentDirection(normalize(wi)), m_Le(Le) {
}

DirectionalLight::DirectionalLight(const Vec3f& wi,
                 const Vec3f& exitantPower,
                 const Scene& scene): DirectionalLight(wi, computeEmittedRadiance(exitantPower, scene)) {
}

Vec3f DirectionalLight::getPowerUpperBound(const Scene& scene) const {
    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);
    return Vec3f(m_Le) * pi<float>() * sqr(radius);
}

void DirectionalLight::exposeIO(GUI& gui, UpdateFlag& flag) {
    if (gui.addVarRW(BNZ_GUI_VAR(m_IncidentDirection))) {
        flag.addUpdate();
    }

    if (gui.addVarRW(BNZ_GUI_VAR(m_Color)) || gui.addVarRW(BNZ_GUI_VAR(m_fLe))) {
        m_Le = m_fLe * m_Color;
        flag.addUpdate();
    }
}

void DirectionalLight::accept(LightVisitor& visitor) {
    visitor.visit(*this);
}

// Sample an exitant ray
Vec3f DirectionalLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf) const
{
    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);

    // Center of the disk
    center += m_IncidentDirection * radius;

    Vec2f pos2D = uniformSampleDisk(raySample, radius);

    auto originPdf = 1.f / (pi<float>() * radius * radius);
    emissionVertexPdf = 1.f;

    auto pdf = originPdf * emissionVertexPdf;

    auto matrix = frameY(center, m_IncidentDirection);
    auto rayOrigin = Vec3f(matrix * Vec4f(pos2D.x, 0, pos2D.y, 1.f));

    exitantRaySample = RaySample(Ray(rayOrigin, -m_IncidentDirection), pdf);

    return m_Le;
}

Vec3f DirectionalLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf,
        Intersection& intersection,
        float& emissionVertexIncidentDirectionJacobian,
        float& intersectionPdfWrtArea) const
{
    auto Le = sampleExitantRay(scene, emissionVertexSample, raySample, exitantRaySample, emissionVertexPdf);

    intersection = scene.intersect(exitantRaySample.value);
    if (intersection) {
        emissionVertexIncidentDirectionJacobian = 0.f;
        auto originPdf = exitantRaySample.pdf; // This is true because the emission vertex pdf is 1
        intersectionPdfWrtArea = originPdf * abs(dot(-exitantRaySample.value.dir, intersection.Ns));
    }
    else {
        emissionVertexIncidentDirectionJacobian = 0;
        intersectionPdfWrtArea = 0;
    }

    return Le;
}

// Sample the direct illumination on point
// Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
// Compute the imageSample associated to lensSample that gived the exitant ray reaching point
Vec3f DirectionalLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const SurfacePoint& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf,
        float& pointPdfWrtArea
) const {
    auto Le = sampleDirectIllumination(scene, emissionVertexSample, point.P, shadowRay, emissionVertexToIncidentDirectionJacobian,
                                       emissionVertexPdf);
    shadowRay = RaySample(Ray(point, shadowRay.value.dir), shadowRay.pdf);

    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);

    pointPdfWrtArea = (1.f / (pi<float>() * sqr(radius))) * abs(dot(shadowRay.value.dir, point.Ns));

    return Le;
}

Vec3f DirectionalLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec3f& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf
) const {
    auto directionPdf = 1.f;
    shadowRay = RaySample(Ray(point, m_IncidentDirection), directionPdf);

    emissionVertexToIncidentDirectionJacobian = 0.f;
    emissionVertexPdf = 1.f;

    return m_Le;
}

Vec3f DirectionalLight::evalBoundedDirectIllumination(
            const Scene& scene,
            const Vec2f& positionSample,
            const SurfacePoint& point,
            float radius,
            Ray& shadowRay
) const {
    shadowRay = Ray(point, m_IncidentDirection);
    return m_Le;
}

void DirectionalLight::sampleVPL(
        const Scene& scene,
        float lightPdf,
        const Vec2f& emissionVertexSample,
        EmissionVPLContainer& container) const {
    container.add(DirectionalLightVPL{
                      m_IncidentDirection,
                      m_Le / lightPdf
                  });
}

}
