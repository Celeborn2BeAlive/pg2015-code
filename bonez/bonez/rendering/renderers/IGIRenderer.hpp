#pragma once

#include "TileProcessingRenderer.hpp"
#include <bonez/sampling/Random.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>

#include "paths.hpp"

namespace BnZ {

class IGIRenderer: public TileProcessingRenderer {
    struct SurfaceVPL {
        Intersection lastVertex;
        BSDF lastVertexBSDF;
        float pdf; // pdf of the complete path
        float pdfLastVertex; // pdf of the last vertex, conditional to the previous
        Vec3f power;
        uint32_t depth; // number of edges of the path

        void init(const BidirPathVertex& vertex) {
            lastVertex = vertex.intersection();
            lastVertexBSDF = vertex.bsdf();
            pdf = vertex.pathPdf();
            pdfLastVertex = vertex.pdfWrtArea();
            power = vertex.power();
            depth = vertex.length();
        }
    };

    PowerBasedLightSampler m_LightSampler;

    uint32_t m_nMaxPathDepth = 2u;
    uint32_t m_nPathCount = 1u;

    std::vector<SurfaceVPL> m_SurfaceVPLBuffer;

    enum FramebufferTarget {
        FINAL_RENDER,
        FINAL_RENDER_DEPTH1
    };

    Vec3f evalVPLContribution(const SurfaceVPL& vpl, const Intersection& I, const BSDF& bsdf) const;

public:
    void preprocess() override;

    uint32_t getMaxLightPathDepth() const;

    void beginFrame() override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void doExposeIO(GUI& gui) override;

    void doLoadSettings(const tinyxml2::XMLElement& xml) override;

    void doStoreSettings(tinyxml2::XMLElement& xml) const override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void initFramebuffer() override;
};

}
