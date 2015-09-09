#pragma once

#include <bonez/maths/maths.hpp>

#include "bonez/scene/Scene.hpp"
#include "Light.hpp"
#include "LightVisitor.hpp"

namespace BnZ {

class DirectionalLight: public Light {
    float m_fLe;
    Col3f m_Color;
public:
    Vec3f m_IncidentDirection;
    Vec3f m_Le;

    DirectionalLight(const Vec3f& wi,
                     const Vec3f& Le);

    DirectionalLight(const Vec3f& wi,
                     const Vec3f& exitantPower,
                     const Scene& scene);

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
                Ray& shadowRay
    ) const override;

    void sampleVPL(
            const Scene& scene,
            float lightPdf,
            const Vec2f& emissionVertexSample,
            EmissionVPLContainer& container) const override;
};

}
