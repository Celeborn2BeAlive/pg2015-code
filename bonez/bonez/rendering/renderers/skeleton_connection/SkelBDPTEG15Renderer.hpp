#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include <bonez/sampling/multiple_importance_sampling.hpp>
#include <bonez/opengl/GLGBuffer.hpp>
#include <bonez/sampling/distribution1d.h>
#include <bonez/sampling/shapes.hpp>
#include <bonez/sampling/patterns.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include <bonez/opengl/debug/GLDebugRenderer.hpp>

#include "../paths.hpp"
#include "../recursive_mis_bdpt.hpp"
#include "../DirectImportanceSampleTilePartionning.hpp"

namespace BnZ {

class SkelBDPTEG15Renderer: public TileProcessingRenderer {
private:
    using PathVertex = BDPTPathVertex;

    void preprocess() override;

    void beginFrame() override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const;

    void skeletonMapping(const SurfacePoint& I, uint32_t pixelID,
                         Vec4i& nearestNodes, Vec4f& weights) const;

    void computePrimarySkelMap();

    void computeLightVertexDistributions();

    uint32_t getMaxLightPathDepth() const {
        return m_nMaxDepth - 1;
    }

    uint32_t getMaxEyePathDepth() const {
        return m_nMaxDepth;
    }

    float Mis(float pdf) const {
        return pdf; // Balance heuristic
    }

    virtual void doExposeIO(GUI& gui);

    virtual void doLoadSettings(const tinyxml2::XMLElement& xml);

    virtual void doStoreSettings(tinyxml2::XMLElement& xml) const;

    void initFramebuffer() override;

    struct PrimarySkelMap {
        std::vector<Vec4i> nodes;
        std::vector<Vec4f> weights;

        void resizeBuffers(uint32_t size) {
            nodes.resize(size);
            weights.resize(size);
        }
    };

    PrimarySkelMap m_SkelMappingData;

    Array2d<float> m_SkelNodeLightVertexDistributionsArray; // For each node, contains a discrete distribution among light paths
    std::vector<float> m_DefaultLightVertexDistributionsArray; // When a point is not associated with any node, use this distribution
    uint32_t m_nDistributionSize;

    std::vector<EmissionVertex> m_EmissionVertexBuffer;
    Array2d<PathVertex> m_LightPathBuffer;

    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartitionning;

    PowerBasedLightSampler m_LightSampler;

    Shared<const CurvilinearSkeleton> m_pSkel;

    uint32_t m_nMaxDepth = 3;

    uint32_t m_nLightPathCount = 1024;
    float m_fPowerSkelFactor = 0.1f;
    uint32_t m_nMaxNodeCount = 4;
    bool m_bUseSkelGridMapping = false;
    bool m_bUseSkeleton = true;
    bool m_bUniformResampling = false;

    enum FramebufferTarget {
        FINAL_RENDER,
        SELECTED_NODE0,
        SELECTED_NODE1,
        SELECTED_NODE2,
        SELECTED_NODE3,
        FINAL_RENDER_DEPTH1
    };
};

}
