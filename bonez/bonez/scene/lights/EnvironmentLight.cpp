#include "EnvironmentLight.hpp"

#include <bonez/scene/Scene.hpp>
#include <bonez/sampling/distribution2d.h>
#include <bonez/sampling/shapes.hpp>

namespace BnZ {

EnvironmentLight::EnvironmentLight(Image LeMap, const Vec3f& LeScale, const Mat3f& worldToLocalMatrix):
    m_LeMap(std::move(LeMap)),
    m_RcpMapSize(1.f / m_LeMap.getWidth(), 1.f / m_LeMap.getHeight()),
    m_LeScale(LeScale),
    m_WorldToLocalMatrix(worldToLocalMatrix),
    m_LocalToWorldMatrix(inverse(worldToLocalMatrix)) {

    buildSamplingDistribution();
}

EnvironmentLight::EnvironmentLight(const Vec2u& resolution, Vec3f wi, const Vec3f& exitantPower, const Scene& scene):
    m_LeMap(resolution.x, resolution.y),
    m_RcpMapSize(1.f / m_LeMap.getWidth(), 1.f / m_LeMap.getHeight()),
    m_LeScale(1.f),
    m_WorldToLocalMatrix(1.f),
    m_LocalToWorldMatrix(1.f) {
    m_LeMap.fill(Vec4f(0, 0, 0, 1));

    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);

    wi = normalize(wi);

    auto uv = rcpSphericalMapping(wi);
    auto pixel = getPixel(uv, m_LeMap.getSize());

    Vec3f Le = float(m_LeMap.getPixelCount()) * exitantPower * rcpSphericalMappingJacobian(wi) / (pi<float>() * sqr(radius));
    m_LeMap(pixel.x, pixel.y) = Vec4f(Le, 1.f);

    buildSamplingDistribution();
}

void EnvironmentLight::buildSamplingDistribution() {
    m_SamplingDistribution.resize(getDistribution2DBufferSize(m_LeMap.getWidth(), m_LeMap.getHeight()));
    auto scale = pi<float>() * m_RcpMapSize.y;
    buildDistribution2D([&](size_t x, size_t y) {
        float sinTheta = sin((y + 0.5f) * scale);
        return sinTheta * luminance(Vec3f(m_LeMap(x, y)));
    }, m_SamplingDistribution.data(), m_LeMap.getWidth(), m_LeMap.getHeight());
}

Vec3f EnvironmentLight::getPowerUpperBound(const Scene& scene) const {
    Vec3f sum = zero<Vec3f>();
    for(auto y: range(m_LeMap.getHeight())) {
        for(auto x: range(m_LeMap.getWidth())) {
            Vec2f uv = Vec2f(x + 0.5f, y + 0.5f) * m_RcpMapSize;
            sum += Vec3f(m_LeMap(x, y)) * sphericalMappingJacobian(uv);
        }
    }
    sum /= m_LeMap.getPixelCount();

    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);

    return sum * pi<float>() * sqr(radius) * m_LeScale;
}

void EnvironmentLight::exposeIO(GUI& gui, UpdateFlag& flag) {
    if (gui.addVarRW(BNZ_GUI_VAR(m_LeScale))) {
        flag.addUpdate();
    }
}

void EnvironmentLight::accept(LightVisitor& visitor) {
    visitor.visit(*this);
}

// Sample an exitant ray
Vec3f EnvironmentLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf) const {
    auto imageSample = sampleContinuousDistribution2D(m_SamplingDistribution.data(),
                                                      m_LeMap.getWidth(), m_LeMap.getHeight(), emissionVertexSample);
    auto uv = imageSample.value * m_RcpMapSize;
    auto wi = sphericalMapping(uv);
    auto wo = - m_LocalToWorldMatrix * wi;
    emissionVertexPdf = imageSample.pdf * rcpSphericalMappingJacobian(wi);

    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);
    auto frame = frameZ(wo);

    auto diskSample = uniformSampleDisk(raySample, radius);

    auto position = center + diskSample.x * frame[0] + diskSample.y * frame[1] - radius * wo;
    auto originPdf = 1.f / (pi<float>() * sqr(radius));

    exitantRaySample = RaySample(Ray(position, wo), emissionVertexPdf * originPdf);

    auto pixel = getPixel(uv, m_LeMap.getSize());
    return m_LeScale * Vec3f(m_LeMap(pixel.x, pixel.y));
}

// Sample an exitant ray and compute its intersection with the scene
Vec3f EnvironmentLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf,
        Intersection& intersection,
        float& emissionVertexIncidentDirectionJacobian,
        float& intersectionPdfWrtArea) const {
    auto Le = sampleExitantRay(scene, emissionVertexSample, raySample, exitantRaySample, emissionVertexPdf);
    intersection = scene.intersect(exitantRaySample.value);
    if(!intersection) {
        emissionVertexIncidentDirectionJacobian = 0.f;
        intersectionPdfWrtArea = 0.f;
    } else {
        emissionVertexIncidentDirectionJacobian = 1.f;
        auto originPdf = exitantRaySample.pdf / emissionVertexPdf;
        intersectionPdfWrtArea = originPdf * abs(dot(-exitantRaySample.value.dir, intersection.Ns));
    }
    return Le;
}

// Sample the direct illumination on point
// Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
Vec3f EnvironmentLight::sampleDirectIllumination(
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

// Sample the direct illumination on a point of the empty space
// Compute a shadow ray sample on the sphere of point with pdf wrt solid angle
Vec3f EnvironmentLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec3f& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf
) const {
    auto imageSample = sampleContinuousDistribution2D(m_SamplingDistribution.data(),
                                                      m_LeMap.getWidth(), m_LeMap.getHeight(), emissionVertexSample);
    auto uv = imageSample.value * m_RcpMapSize;
    auto wi = m_LocalToWorldMatrix * sphericalMapping(uv);
    auto directionPdf = imageSample.pdf * rcpSphericalMappingJacobian(wi);

    shadowRay = RaySample(Ray(point, wi), directionPdf);

    emissionVertexToIncidentDirectionJacobian = 1.f;
    emissionVertexPdf = directionPdf;

    auto pixel = getPixel(uv, m_LeMap.getSize());
    return m_LeScale * Vec3f(m_LeMap(pixel.x, pixel.y));
}

Vec3f EnvironmentLight::evalBoundedDirectIllumination(
        const Scene& scene,
        const Vec2f& positionSample,
        const SurfacePoint& point,
        float radius,
        Ray& shadowRay) const {
    auto imageSample = sampleContinuousDistribution2D(m_SamplingDistribution.data(),
                                                      m_LeMap.getWidth(), m_LeMap.getHeight(), positionSample);
    auto wi = m_LocalToWorldMatrix * sphericalMapping(imageSample.value);
    shadowRay = Ray(point, wi);

    auto pixel = getPixel(imageSample.value, m_LeMap.getSize());
    return m_LeScale * Vec3f(m_LeMap(pixel.x, pixel.y));
}

Vec3f EnvironmentLight::Le(const Vec3f& wi) const {
    auto localWi = m_WorldToLocalMatrix * wi;
    auto uv = rcpSphericalMapping(localWi);
    auto pixel = getPixel(uv, m_LeMap.getSize());
    return m_LeScale * Vec3f(m_LeMap(pixel.x, pixel.y));
}

void EnvironmentLight::pdf(const Scene& scene, const Vec3f& wi, float& emissionVertexPdf, float& exitantPdf) const {
    Vec3f center;
    float radius;
    boundingSphere(scene.getBBox(), center, radius);
    exitantPdf = 1.f / (pi<float>() * sqr(radius));

    auto localWi = m_WorldToLocalMatrix * wi;
    auto uv = rcpSphericalMapping(localWi);
    auto point = uv * Vec2f(m_LeMap.getSize());

    emissionVertexPdf = pdfContinuousDistribution2D(m_SamplingDistribution.data(), m_LeMap.getWidth(), m_LeMap.getHeight(), point) * rcpSphericalMappingJacobian(wi);
}

void EnvironmentLight::sampleVPL(
        const Scene& scene,
        float lightPdf,
        const Vec2f& emissionVertexSample,
        EmissionVPLContainer& container) const {
    auto imageSample = sampleContinuousDistribution2D(m_SamplingDistribution.data(),
                                                      m_LeMap.getWidth(), m_LeMap.getHeight(), emissionVertexSample);
    auto uv = imageSample.value * m_RcpMapSize;
    auto wi = m_LocalToWorldMatrix * sphericalMapping(uv);
    auto directionPdf = imageSample.pdf * rcpSphericalMappingJacobian(wi);

    auto pixel = getPixel(imageSample.value, m_LeMap.getSize());

    container.add(DirectionalLightVPL {
                    wi,
                    m_LeScale * Vec3f(m_LeMap(pixel.x, pixel.y)) / (lightPdf * directionPdf)
                  });
}

}
