#pragma once

#include <bonez/types.hpp>
#include <bonez/scene/Scene.hpp>
#include <bonez/scene/SurfacePoint.hpp>
#include <bonez/scene/Ray.hpp>

namespace BnZ {

class PowerBasedLightSampler {
public:
    virtual ~PowerBasedLightSampler() {
    }

    void initFrame(const Scene& scene);

    const Light* sample(const Scene& scene, float lightSample, float& pdf, uint32_t* pLightID = nullptr) const;

    float pdf(uint32_t lightID) const;

    void computeEnvironmentLightsPdf(const Scene& scene, const Vec3f& wi, float& emissionVertexPdf, float& exitantPdf) const;

private:
    UpdateFlag m_UpdateFlag;
    DiscreteDistribution m_PowerBasedDistribution;
};

}
