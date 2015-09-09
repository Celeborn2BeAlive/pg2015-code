#pragma once

#include "Light.hpp"

namespace BnZ {

class Scene;
struct TriangleMesh;
struct Material;

class AreaLight: public Light {
public:
    AreaLight(uint32_t meshIdx, const TriangleMesh& mesh, const Material& material);

    Vec3f getPowerUpperBound(const Scene& scene) const override;

    void exposeIO(GUI& gui, UpdateFlag& flag) override;

    void accept(LightVisitor& visitor) override;

    uint32_t getMeshIdx() const {
        return m_nMeshIdx;
    }

    // Sample a point and return the emitted exitant radiance at that point
    Vec3f sample(const Scene& scene, float s1D, const Vec2f& s2D,
                 SurfacePointSample& surfacePoint) const;

    Vec3f sample(const Scene& scene, Vec2f s2D,
                 SurfacePointSample& surfacePoint) const;

    float pdf(const SurfacePoint& surfacePoint, const Scene& scene) const;

    void pdf(const SurfacePoint& surfacePoint, const Vec3f& outgoingDirection,
                const Scene& scene, float& pointPdf, float& directionPdf) const;

    Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec2f& raySample,
            RaySample& exitantRaySample,
            float& emissionVertexPdf) const;

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
            Ray& shadowRay
    ) const override;

    void sampleVPL(
            const Scene& scene,
            float lightPdf,
            const Vec2f& emissionVertexSample,
            EmissionVPLContainer& container) const override;

private:
    uint32_t m_nMeshIdx;
    Vec3f m_Power;
};

}
