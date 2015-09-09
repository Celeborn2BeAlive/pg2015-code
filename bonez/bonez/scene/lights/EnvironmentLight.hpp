#pragma once

#include "Light.hpp"

#include <bonez/image/Image.hpp>

namespace BnZ {

class EnvironmentLight: public Light {
public:
    EnvironmentLight(Image LeMap, const Vec3f& LeScale, const Mat3f& worldToLocalMatrix = Mat3f(1.f));

    EnvironmentLight(const Vec2u& resolution, Vec3f wi, const Vec3f& exitantPower, const Scene& scene);

    Vec3f getPowerUpperBound(const Scene& scene) const override;

    void exposeIO(GUI& gui, UpdateFlag& flag) override;

    void accept(LightVisitor& visitor) override;

    Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec2f& raySample,
            RaySample& exitantRaySample,
            float& emissionVertexPdf) const override;

    Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec2f& raySample,
            RaySample& exitantRaySample,
            float& emissionVertexPdf,
            Intersection& intersection,
            float& emissionVertexIncidentDirectionJacobian,
            float& intersectionPdfWrtArea) const override;

    Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const SurfacePoint& point,
            RaySample& shadowRay,
            float& emissionVertexToIncidentDirectionJacobian,
            float& emissionVertexPdf,
            float& pointPdfWrtArea
    ) const override;

    Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec3f& point,
            RaySample& shadowRay,
            float& emissionVertexToIncidentDirectionJacobian,
            float& emissionVertexPdf
    ) const override;

    Vec3f evalBoundedDirectIllumination(
            const Scene& scene,
            const Vec2f& positionSample,
            const SurfacePoint& point,
            float radius,
            Ray& shadowRay) const override;

    Vec3f Le(const Vec3f& wi) const;

    // Compute the probability density of sampling an EmissionVertex corresponding to the incident direction wi
    // The pdf of an exitant ray in that direction is emissionVertexPdf * exitantPdf
    void pdf(const Scene& scene, const Vec3f& wi, float& emissionVertexPdf, float& exitantPdf) const;

    void sampleVPL(
            const Scene& scene,
            float lightPdf,
            const Vec2f& emissionVertexSample,
            EmissionVPLContainer& container) const override;

private:
    void buildSamplingDistribution();

    Image m_LeMap;
    Vec2f m_RcpMapSize;
    Vec3f m_LeScale;
    Mat3f m_WorldToLocalMatrix;
    Mat3f m_LocalToWorldMatrix;

    std::vector<float> m_SamplingDistribution;
};

}
