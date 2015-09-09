#pragma once

#include <bonez/rendering/renderers/TileProcessingRenderer.hpp>
#include <bonez/sampling/multiple_importance_sampling.hpp>
#include <bonez/opengl/GLGBuffer.hpp>
#include <bonez/sampling/distribution1d.h>
#include <bonez/sampling/shapes.hpp>
#include <bonez/sampling/patterns.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/shading/BSDF.hpp>

#include <bonez/opengl/debug/GLDebugRenderer.hpp>

#include <bonez/utils/HashGrid.hpp>
#include <bonez/utils/MultiDimensionalArray.hpp>

#include "DirectImportanceSampleTilePartionning.hpp"
#include "paths.hpp"

namespace BnZ {

class VCMRenderer: public TileProcessingRenderer {
    struct PathVertex {
        Intersection m_Intersection;
        BSDF m_BSDF;
        uint32_t m_nScatteringEvent; // The event that has generated the vertex
        float m_fPathPdf; // pdf of the complete path
        float m_fPdfWrtArea; // pdf of the last vertex, conditional to the previous
        Vec3f m_Power;
        uint32_t m_nDepth; // number of edges of the path

        float m_fdVCM;
        float m_fdVC;
        float m_fdVM;

        void init(const BidirPathVertex& vertex) {
            m_Intersection = vertex.intersection();
            m_BSDF = vertex.bsdf();
            m_nScatteringEvent = vertex.sampledEvent();
            m_fPathPdf = vertex.pathPdf();
            m_fPdfWrtArea = vertex.pdfWrtArea();
            m_Power = vertex.power();
            m_nDepth = vertex.length();
        }
    };

    void preprocess() override;

    void beginFrame() override;

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override;

    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const;

    void sampleLightPath(
            const Scene& scene,
            uint32_t threadID, uint32_t pixelID, PathVertex* pBuffer,
            uint32_t maxLightPathDepth) const;

    void connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const;

    void computeEmittedRadiance(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                           uint32_t x, uint32_t y, const PathVertex& eyeVertex) const;

    void computeDirectIllumination(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                                   uint32_t x, uint32_t y, const PathVertex& eyeVertex,
                                   const Light& light, float lightPdf) const;

    void connectVertices(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                         uint32_t x, uint32_t y, const PathVertex& eyeVertex,
                         const PathVertex& lightVertex) const;

    void vertexConnection(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y,
                                    const Light* pLight, float lightPdf, const PathVertex* pLightPath,
                                    const PathVertex& eyeVertex) const;

    void vertexMerging(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                                     const PathVertex& eyeVertex) const;

    friend const Vec3f& getPosition(const PathVertex& pathVertex) {
        return pathVertex.m_Intersection.P;
    }

    friend bool isValid(const PathVertex& pathVertex) {
        return pathVertex.m_fPathPdf > 0.f;
    }

    float Mis(float pdf) const {
        return pdf; // Balance heuristic
    }

    std::string getAlgorithmType() const;

    void setAlgorithmType(const std::string& algorithmType);

    uint32_t getMaxLightPathDepth() const;

    uint32_t getMaxEyePathDepth() const;

    virtual void doExposeIO(GUI& gui);

    virtual void doLoadSettings(const tinyxml2::XMLElement& xml);

    virtual void doStoreSettings(tinyxml2::XMLElement& xml) const;

    void initFramebuffer() override;

    uint32_t m_nMaxDepth = 3;

    PowerBasedLightSampler m_LightSampler;

    Array2d<PathVertex> m_LightPathBuffer;
    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartitionning;

    uint32_t m_nLightPathCount;
    uint32_t m_nLightVertexCount;

    HashGrid m_LightVerticesHashGrid;

    enum AlgorithmType
    {
        // light vertices contribute to camera,
        // No MIS weights (dVCM, dVM, dVC all ignored)
        kLightTrace = 0,

        // Camera and light vertices merged on first non-specular surface from camera.
        // Cannot handle mixed specular + non-specular materials.
        // No MIS weights (dVCM, dVM, dVC all ignored)
        kPpm,

        // Camera and light vertices merged on along full path.
        // dVCM and dVM used for MIS
        kBpm,

        // Standard bidirectional path tracing
        // dVCM and dVC used for MIS
        kBpt,

        // Vertex connection and mering
        // dVCM, dVM, and dVC used for MIS
        kVcm
    };

    AlgorithmType m_AlgorithmType = kVcm;
    bool  m_UseVM;             // Vertex merging (of some form) is used
    bool  m_UseVC;             // Vertex connection (BPT) is used
    bool  m_LightTraceOnly;    // Do only light tracing
    bool  m_Ppm;               // Do PPM, same terminates camera after first merge

    float m_SceneRadius;
    float m_RadiusAlpha = 0.75;       // Radius reduction rate parameter
    float m_RadiusFactor = 0.03;
    float m_BaseRadius;        // Initial merging radius
    float m_MisVmWeightFactor; // Weight of vertex merging (used in VC)
    float m_MisVcWeightFactor; // Weight of vertex connection (used in VM)
    float m_VmNormalization;   // 1 / (Pi * radius^2 * light_path_count)

    uint32_t m_nIterationCount = 0u;
};

}
