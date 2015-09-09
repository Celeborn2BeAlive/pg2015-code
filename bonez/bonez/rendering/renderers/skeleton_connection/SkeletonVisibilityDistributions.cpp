#include "SkeletonVisibilityDistributions.hpp"

namespace BnZ {

void buildDistributions(
        SkeletonVisibilityDistributions& distributions,
        const Scene& scene,
        const std::vector<GraphNodeIndex>& skeletonNodes,
        const EmissionVertex* pEmissionVertexArray,
        const Array2d<BDPTPathVertex>& surfaceLightVertexArray,
        std::size_t lightPathCount,
        bool useNodeRadianceScale,
        bool useNodeDistanceScale,
        std::size_t threadCount) {
    auto maxDepth = surfaceLightVertexArray.size(0);
    auto pathCount = lightPathCount;

    auto evalDefaultConservativeWeight = [&](std::size_t depth, std::size_t pathIdx) {
        if(!depth) {
            if(!pEmissionVertexArray[pathIdx].m_pLight ||
                    pEmissionVertexArray[pathIdx].m_fLightPdf == 0.f) {
                return 0.f;
            }
            return 1.f;
        }
        if(surfaceLightVertexArray(depth - 1u, pathIdx).m_fPathPdf == 0.f) {
            return 0.f;
        }
        return 1.f;
    };

    auto evalNodeVisibilityWeight = [&](std::size_t depth, std::size_t pathIdx, const Vec3f& nodePosition, float nodeMaxballRadius) {
        if(!depth) {
            if(!pEmissionVertexArray[pathIdx].m_pLight ||
                    pEmissionVertexArray[pathIdx].m_fLightPdf == 0.f) {
                return 0.f;
            }

            RaySample shadowRaySample;
            auto L = pEmissionVertexArray[pathIdx].m_pLight->sampleDirectIllumination(
                        scene,
                        pEmissionVertexArray[pathIdx].m_PositionSample,
                        nodePosition,
                        shadowRaySample);

            if(L == zero<Vec3f>() || shadowRaySample.pdf == 0.f) {
                return 0.f;
            }
            if(scene.occluded(shadowRaySample.value)) {
                return 0.f;
            }

            Vec3f weight(1.f);

            if(useNodeRadianceScale) {
                weight *= L;
            }

            if(useNodeDistanceScale) {
                weight /= shadowRaySample.pdf;
            }

            return luminance(weight);
        }
        auto& lightVertex = surfaceLightVertexArray(depth - 1, pathIdx);
        if(lightVertex.m_fPathPdf == 0.f) {
            return 0.f;
        }

        auto& I = lightVertex.m_Intersection;
        auto dir = nodePosition - I.P;
        auto l = BnZ::length(dir);
        dir /= l;

        if(dot(I.Ns, dir) <= 0.f || scene.occluded(Ray(I, dir, l))) {
            return 0.f;
        }

        Vec3f weight(1.f);

        if(useNodeRadianceScale) {
            weight *= lightVertex.m_Power;
        }

        if(useNodeDistanceScale) {
            weight /= sqr(l);
        }

        return luminance(weight);
    };

    auto evalNodeGeometryWeight = [&](std::size_t depth, std::size_t pathIdx, const Vec3f& nodePosition, float nodeMaxballRadius) {
        if(!depth) {
            if(!pEmissionVertexArray[pathIdx].m_pLight ||
                    pEmissionVertexArray[pathIdx].m_fLightPdf == 0.f) {
                return 0.f;
            }

            RaySample shadowRaySample;
            auto L = pEmissionVertexArray[pathIdx].m_pLight->sampleDirectIllumination(
                        scene,
                        pEmissionVertexArray[pathIdx].m_PositionSample,
                        nodePosition,
                        shadowRaySample);

            if(L == zero<Vec3f>() || shadowRaySample.pdf == 0.f) {
                return 0.f;
            }

            Vec3f weight(1.f);

            if(useNodeRadianceScale) {
                weight *= L;
            }

            if(useNodeDistanceScale) {
                weight /= shadowRaySample.pdf;
            }

            return luminance(weight);
        }
        auto& lightVertex = surfaceLightVertexArray(depth - 1, pathIdx);
        if(lightVertex.m_fPathPdf == 0.f) {
            return 0.f;
        }

        auto& I = lightVertex.m_Intersection;
        auto dir = nodePosition - I.P;
        auto l = BnZ::length(dir);
        dir /= l;

        if(dot(I.Ns, dir) <= 0.f) {
            return 0.f;
        }

        Vec3f weight(1.f);

        if(useNodeRadianceScale) {
            weight *= lightVertex.m_Power;
        }

        if(useNodeDistanceScale) {
            weight /= sqr(l);
        }

        return luminance(weight);
    };

    auto evalNodeConservativeWeight = [&](std::size_t depth, std::size_t pathIdx, const Vec3f& nodePosition, float nodeMaxballRadius) {
        if(!depth) {
            if(!pEmissionVertexArray[pathIdx].m_pLight ||
                    pEmissionVertexArray[pathIdx].m_fLightPdf == 0.f) {
                return 0.f;
            }
            return 1.f;
        }
        if(surfaceLightVertexArray(depth - 1u, pathIdx).m_fPathPdf == 0.f) {
            return 0.f;
        }
        return 1.f;
    };

    distributions.buildDistributions(*scene.getCurvSkeleton(), skeletonNodes, pathCount, maxDepth, threadCount,
                              evalDefaultConservativeWeight,
                              evalNodeVisibilityWeight,
                              evalNodeGeometryWeight,
                              evalNodeConservativeWeight);
}

void buildDistributions(
        SkeletonVisibilityDistributions& distributions,
        const Scene& scene,
        const std::vector<GraphNodeIndex>& skeletonNodes,
        const SurfacePointSample* surfacePointSamples,
        std::size_t surfacePointSampleCount,
        bool useNodeDistanceScale,
        std::size_t threadCount) {
    auto maxDepth = 0u;
    auto pathCount = surfacePointSampleCount;

    auto evalDefaultConservativeWeight = [&](std::size_t depth, std::size_t pathIdx) {
        if(surfacePointSamples[pathIdx].pdf == 0.f) {
            return 0.f;
        }
        return 1.f;
    };

    auto evalNodeVisibilityWeight = [&](std::size_t depth, std::size_t pathIdx, const Vec3f& nodePosition, float nodeMaxballRadius) {
        auto& lightVertex = surfacePointSamples[pathIdx];
        if(lightVertex.pdf == 0.f) {
            return 0.f;
        }

        auto& I = lightVertex.value;
        auto dir = nodePosition - I.P;
        auto l = BnZ::length(dir);
        dir /= l;

        if(dot(I.Ns, dir) <= 0.f || scene.occluded(Ray(I, dir, l))) {
            return 0.f;
        }

        Vec3f weight(1.f);

        if(useNodeDistanceScale) {
            weight /= sqr(l);
        }

        return luminance(weight);
    };

    auto evalNodeConservativeWeight = [&](std::size_t depth, std::size_t pathIdx, const Vec3f& nodePosition, float nodeMaxballRadius) {
        if(surfacePointSamples[pathIdx].pdf == 0.f) {
            return 0.f;
        }
        return 1.f;
    };

    distributions.buildDistributions(*scene.getCurvSkeleton(), skeletonNodes, pathCount, maxDepth, threadCount,
                              evalDefaultConservativeWeight,
                              evalNodeVisibilityWeight,
                              evalNodeConservativeWeight);
}

//Sample1u SkeletonVisibilityDistributions::sampleDistribution(GraphNodeIndex nodeIdx,
//                            std::size_t depth,
//                            float distribSample) const {
//    auto validDistribCount = size_t(0);
//    for(const auto& distrib : m_PerDepthPerNodeDistributions) {
//        auto pDistribution = distrib.getSlicePtr(depth, nodeIdx);
//        if(cdfDiscreteDistribution1D(pDistribution, m_nLightPathCount)) {
//            ++validDistribCount;
//        }
//    }
//    if(validDistribCount) {
//        auto distribIdx = clamp(size_t(validDistribCount * distribSample), size_t(0), validDistribCount - 1);
//        auto j = size_t(0);
//        for(auto i : range(m_PerDepthPerNodeDistributions.size())) {
//            auto pDistribution = m_PerDepthPerNodeDistributions[i].getSlicePtr(depth, nodeIdx);
//            if(cdfDiscreteDistribution1D(pDistribution, m_nLightPathCount)) {
//                if(j == distribIdx) {
//                    return Sample1u(i, 1.f / validDistribCount);
//                }
//                ++j;
//            }
//        }
//    }
//    return Sample1u(0, 0);
//}

Sample1u SkeletonVisibilityDistributions::sample(std::size_t distribIdx,  GraphNodeIndex nodeIdx,
                std::size_t depth, float lightVertexSample) const {
    auto pDistribution = m_PerDepthPerNodeDistributions[distribIdx].getSlicePtr(depth, nodeIdx);
    return sampleDiscreteDistribution1D(pDistribution, m_nLightPathCount, lightVertexSample);
}

Sample1u SkeletonVisibilityDistributions::sampleCombined(std::size_t distribIdx, GraphNodeIndex* pNodeIndexBuffer, std::size_t nodeCount,
                        std::size_t depth, float lightVertexSample) const {
    return sampleCombinedDiscreteDistribution(nodeCount, [&](size_t i) {
        return m_PerDepthPerNodeDistributions[distribIdx].getSlicePtr(depth, pNodeIndexBuffer[i]);
    }, m_nLightPathCount, lightVertexSample);
}

Sample1u SkeletonVisibilityDistributions::sampleDefaultDistribution(std::size_t depth, float lightVertexSample) const {
    auto pDistribution = m_PerDepthDefaulConservativeDistributionsArray.getSlicePtr(depth);
    return sampleDiscreteDistribution1D(pDistribution, m_nLightPathCount, lightVertexSample);
}

float SkeletonVisibilityDistributions::evalResamplingMISWeight(std::size_t distribIdx, GraphNodeIndex nodeIdx, std::size_t depth, const Sample1u& lightVertexSample) const {
    float weight = 1.f;

    // Max heuristic
    for(auto distribIdx2: range(m_PerDepthPerNodeDistributions.size())) {
        auto pDistribution = m_PerDepthPerNodeDistributions[distribIdx2].getSlicePtr(depth, nodeIdx);
        auto pdf = pdfDiscreteDistribution1D(pDistribution, lightVertexSample.value);
        if(pdf > lightVertexSample.pdf || (pdf == lightVertexSample.pdf && distribIdx2 < distribIdx)) {
            weight = 0.f;
            break;
        }
    }

    return weight;
}

float SkeletonVisibilityDistributions::evalCombinedResamplingMISWeight(std::size_t distribIdx, GraphNodeIndex* pNodeIndexBuffer, std::size_t nodeCount, std::size_t depth,
                                      const Sample1u& lightVertexSample) const {
    float weight = 1.f;

    // Max heuristic
    for(auto distribIdx2: range(m_PerDepthPerNodeDistributions.size())) {
        auto pdf = pdfCombinedDiscreteDistribution1D(nodeCount, [&](size_t i) {
            return m_PerDepthPerNodeDistributions[distribIdx2].getSlicePtr(depth, pNodeIndexBuffer[i]);
        }, lightVertexSample.value);
        if(pdf > lightVertexSample.pdf || (pdf == lightVertexSample.pdf && distribIdx2 < distribIdx)) {
            weight = 0.f;
            break;
        }
    }

    return weight;
}

}
