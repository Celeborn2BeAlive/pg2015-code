#pragma once

#include "TileProcessingRenderer.hpp"

#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>

namespace BnZ {

class InstantRadiosityRenderer: public TileProcessingRenderer {
private:
    struct EmissionVPL {
        const Light* pLight;
        float lightPdf;
        Vec2f emissionVertexSample;
    };

    struct SurfaceVPL {
        Intersection intersection;
        BSDF bsdf;
        Vec3f power;

        SurfaceVPL(const Intersection& I, const Vec3f& wi, const Scene& scene, const Vec3f& power):
            intersection(I), bsdf(wi, intersection, scene), power(power) {
        }
    };

    void preprocess() override;

    void beginFrame() override;

    void processPixel(uint32_t threadID, uint32_t tileID, const Vec2u& pixel) const;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void initFramebuffer() override;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    std::size_t m_nLightPathCount = 64;
    std::size_t m_nMaxDepth = 3u;

    std::vector<EmissionVPL> m_EmissionVPLBuffer;
    std::vector<SurfaceVPL> m_SurfaceVPLBuffer;
    PowerBasedLightSampler m_Sampler;
};

}
