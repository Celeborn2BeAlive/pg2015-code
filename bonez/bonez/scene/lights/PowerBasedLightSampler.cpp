#include "PowerBasedLightSampler.hpp"

#include "Light.hpp"

namespace BnZ {

void PowerBasedLightSampler::initFrame(const Scene& scene) {
    if(scene.getLightContainer().hasChanged(m_UpdateFlag)) {
        m_PowerBasedDistribution = computePowerBasedDistribution(scene);
    }
}

const Light* PowerBasedLightSampler::sample(const Scene& scene, float lightSample, float& pdf, uint32_t* pLightID) const {
    const auto& lights = scene.getLightContainer();
    if(lights.empty()) {
        pdf = 0.f;
        return nullptr;
    }

    auto s = m_PowerBasedDistribution.sample(lightSample);
    auto pLight = lights.getLight(s.value);

    pdf = s.pdf;

    if(pLightID) {
        *pLightID = s.value;
    }

    return pLight.get();
}

float PowerBasedLightSampler::pdf(uint32_t lightID) const {
    return m_PowerBasedDistribution.pdf(lightID);
}

void PowerBasedLightSampler::computeEnvironmentLightsPdf(const Scene& scene, const Vec3f& wi,
                                                         float& emissionVertexPdf, float& exitantPdf) const {
    const auto& envLightIndices = scene.getEnvironmentLightIndices();
    const auto& lights = scene.getLightContainer();

    float sumLightPdfs = 0.f;
    for(auto i: envLightIndices) {
        sumLightPdfs += pdf(i);
    }

    emissionVertexPdf = 0.f;
    exitantPdf = 0.f;

    for(auto i: envLightIndices) {
        float oPdf, dPdf;
        auto pEnvLight = static_cast<EnvironmentLight*>(lights.getLight(i).get());
        pEnvLight->pdf(scene, wi, oPdf, dPdf);
        auto lightPdf = pdf(i);
        emissionVertexPdf += lightPdf * oPdf;
        exitantPdf += (lightPdf / sumLightPdfs) * dPdf;
    }
}

}
