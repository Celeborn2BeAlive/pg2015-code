#include "SkeletonMappingRenderer.hpp"

#include <bonez/opengl/debug/GLDebugRenderer.hpp>

namespace BnZ {

void SkeletonMappingRenderer::preprocess() {
    m_pSkeleton = getScene().getCurvSkeleton();

//    if(m_pSkeleton) {
//        auto N = max(1u, uint32_t(sqrt(m_nPointPerNode)));
//        JitteredDistribution2D sampler2D(N, N);

//        m_SkeletonNodePointSampleBuffer.resize(m_pSkeleton->size(), N * N);

//        processTasksDeterminist(m_pSkeleton->size(), [&](uint32_t threadID, uint32_t nodeID) {
//            const auto& node = m_pSkeleton->getNode(nodeID);

//            auto pSamplePoint = m_SkeletonNodePointSampleBuffer.getBufferPtr(nodeID);

//            for(auto j: range(N * N)) {
//                auto s2D = sampler2D.getSample(j, getFloat2(threadID));
//                auto directionSample = uniformSampleSphere(s2D.x, s2D.y);

//                pSamplePoint->m_Intersection = getScene().intersect(Ray(node.P, directionSample.value));
//                pSamplePoint->m_Node = nodeID;
//                pSamplePoint->m_fDotIncidentDirection = pSamplePoint->m_Intersection ? dot(-directionSample.value, pSamplePoint->m_Intersection.Ns) : 0.f;

//                ++pSamplePoint;
//            }
//        }, getThreadCount());

//        auto getPosition = [&](uint32_t i) {
//            return m_SkeletonNodePointSampleBuffer[i].m_Intersection.P;
//        };
//        auto isValid = [&](uint32_t i) {
//            return bool(m_SkeletonNodePointSampleBuffer[i].m_Intersection);
//        };

//        m_SkeletonSamplesKdTree.build(m_SkeletonNodePointSampleBuffer.completeSize(), getPosition, isValid);
//    }
}

void SkeletonMappingRenderer::beginFrame() {

}

void SkeletonMappingRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    if(!m_pSkeleton) {
        return;
    }

    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        auto pixelSample = (Vec2f(x, y) + getPixelSample(threadID, sampleID)) / Vec2f(getFramebufferSize());
        RaySample raySample;
        auto I = tracePrimaryRay(getSensor(), getScene(), getFloat2(threadID), pixelSample, raySample);

        if(I) {
            // Grid mapping
            {
                auto node = m_pSkeleton->getNearestNode(I);
                if(node != UNDEFINED_NODE) {
                    accumulate(GRID_MAPPING, pixelID, Vec4f(getColor(node), 1.f));
                } else {
                    accumulate(GRID_MAPPING, pixelID, Vec4f(Vec3f(0.f), 1.f));
                }
            }

            // Grid with incident direction mapping
            {
                auto node = m_pSkeleton->getNearestNode(I, -raySample.value.dir);
                if(node != UNDEFINED_NODE) {
                    accumulate(GRID_MAPPING_INCIDENT_DIR, pixelID, Vec4f(getColor(node), 1.f));
                } else {
                    accumulate(GRID_MAPPING_INCIDENT_DIR, pixelID, Vec4f(Vec3f(0.f), 1.f));
                }
            }

//            // Kdtree mapping
//            {
//                accumulate(KDTREE_MAPPING, pixelID, Vec4f(Vec3f(0.f), 1.f));
//                float scale = 1.f / m_nKdTreeLookupCount;

//                auto predicate = [&](uint32_t i) {
//                    auto dot1 = dot(I.Ns, -raySample.value.dir);
//                    auto dot2 = m_SkeletonNodePointSampleBuffer[i].m_fDotIncidentDirection;
//                    return dot1 * dot2 > 0.f;
//                };

//                auto process = [&](uint32_t i, const Vec3f position, float sqrDist) {
//                    accumulate(KDTREE_MAPPING, pixelID, Vec4f(scale * getColor(m_SkeletonNodePointSampleBuffer[i].m_Node), 0.f));
//                };

//                m_SkeletonSamplesKdTree.searchKNearestNeighbours(I.P, m_nKdTreeLookupCount, predicate, process);
//            }

        } else {
            accumulate(GRID_MAPPING, pixelID, Vec4f(Vec3f(0.f), 1.f));
            accumulate(GRID_MAPPING_INCIDENT_DIR, pixelID, Vec4f(Vec3f(0.f), 1.f));
            accumulate(KDTREE_MAPPING, pixelID, Vec4f(Vec3f(0.f), 1.f));
        }
    });
}

void SkeletonMappingRenderer::drawGLData(const ViewerData& viewerData) {
    if(!viewerData.pickedIntersection) {
        return;
    }

    viewerData.debugRenderer.addStream(&m_GLDebugStream);
    m_GLDebugStream.clearObjects();

//    auto predicate = [&](uint32_t i) {
//        auto dot1 = dot(I.Ns, incidentDirection);
//        auto dot2 = m_SkeletonNodePointSampleBuffer[i].m_fDotIncidentDirection;
//        return dot1 * dot2 > 0.f;
//    };

//    auto process = [&](uint32_t i, const Vec3f position, float sqrDist) {
//        const auto& node = m_pSkeleton->getNode(m_SkeletonNodePointSampleBuffer[i].m_Node);
//        auto color = getColor(m_SkeletonNodePointSampleBuffer[i].m_Node);
//        //m_GLDebugStream.addLine(I.P, node.P, color, color, 3.f);
//        m_GLDebugStream.addArrow(m_SkeletonNodePointSampleBuffer[i].m_Intersection.P,
//                                 m_SkeletonNodePointSampleBuffer[i].m_Intersection.Ns,
//                                 debugRenderer.getArrowLength(),
//                                 debugRenderer.getArrowBase(),
//                                 color);
//        m_GLDebugStream.addLine(m_SkeletonNodePointSampleBuffer[i].m_Intersection.P, node.P, color, color, 3.f);
//    };

//    m_SkeletonSamplesKdTree.searchKNearestNeighbours(I.P, m_nKdTreeLookupCount, predicate, process);

    auto nodeIndex = m_pSkeleton->getNearestNode(viewerData.pickedIntersection, viewerData.incidentDirection);
    if(nodeIndex != UNDEFINED_NODE) {
        const auto& node = m_pSkeleton->getNode(nodeIndex);
        auto color = getColor(nodeIndex);
        m_GLDebugStream.addLine(viewerData.pickedIntersection.P, node.P, color, color, 3.f);
    }
}

void SkeletonMappingRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nKdTreeLookupCount));
}

}
