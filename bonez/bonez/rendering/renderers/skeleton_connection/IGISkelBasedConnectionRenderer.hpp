#pragma once

#include "../TileProcessingRenderer.hpp"
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/sampling/Random.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include "../paths.hpp"

namespace BnZ {

class IGISkelBasedConnectionRenderer: public TileProcessingRenderer {
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
    uint32_t m_nVPLCount;

    enum FramebufferTarget {
        FINAL_RENDER,
        SELECTED_NODE0,
        SELECTED_NODE1,
        SELECTED_NODE2,
        SELECTED_NODE3,
        FINAL_RENDER_DEPTH1
    };

    float m_fPowerSkelFactor = 0.1f;
    uint32_t m_nMaxNodeCount = 4;
    bool m_bUseSkelGridMapping = false;
    bool m_bUseSkeleton = true;
    bool m_bUniformResampling = false;

    Shared<const CurvilinearSkeleton> m_pSkel;

    struct SkelMappingData {
        std::vector<Vec4i> nodes;
        std::vector<Vec4f> weights;

        void resizeBuffers(uint32_t size) {
            nodes.resize(size);
            weights.resize(size);
        }
    };

    SkelMappingData m_SkelMappingData;

    Array2d<float> m_SkelNodeVPLDistributionsArray; // For each node, contains a discrete distribution among vpls
    std::vector<float> m_DefaultVPLDistributionsArray; // When a point is not associated with any node, use this distribution
    uint32_t m_nDistributionSize;

    void skeletonMapping(const SurfacePoint& I, uint32_t pixelID,
                         Vec4i& nearestNodes, Vec4f& weights) const;

    void computeSkeletonMapping();

    void computeSkelNodeVPLDistributions();

    uint32_t getVPLCountPerNode() const;

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
