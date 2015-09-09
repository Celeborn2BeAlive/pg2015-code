#pragma once

#include "TileProcessingRenderer.hpp"
#include <bonez/utils/KdTree.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include <bonez/opengl/debug/GLDebugStream.hpp>

namespace BnZ {

class SkeletonMappingRenderer: public TileProcessingRenderer {
    void preprocess() override;

    void beginFrame() override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void drawGLData(const ViewerData& viewerData) override;

    void doExposeIO(GUI& gui) override;

    enum FramebufferTarget {
        GRID_MAPPING,
        GRID_MAPPING_INCIDENT_DIR,
        KDTREE_MAPPING,
        FRAMEBUFFER_COUNT
    };

    void initFramebuffer() override {
        addFramebufferChannel("grid_mapping");
        addFramebufferChannel("grid_mapping_wrt_incident_dir");
        addFramebufferChannel("kdtree_mapping");
    }

    Shared<const CurvilinearSkeleton> m_pSkeleton;

    struct SkeletonNodePointSample {
        Intersection m_Intersection;
        GraphNodeIndex m_Node;
        float m_fDotIncidentDirection;
    };

    KdTree m_SkeletonSamplesKdTree;
    Array2d<SkeletonNodePointSample> m_SkeletonNodePointSampleBuffer;
    size_t m_nPointPerNode = 128;
    size_t m_nKdTreeLookupCount = 4;

    GLDebugStreamData m_GLDebugStream;
};

}
