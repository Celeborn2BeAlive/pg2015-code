#include "SkelBDPTEG15Renderer.hpp"
#include <bonez/scene/lights/AreaLight.hpp>

#include "../paths.hpp"
#include "skel_utils.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void SkelBDPTEG15Renderer::preprocess() {
    m_LightSampler.initFrame(getScene());

    if(m_bUseSkeleton) {
        m_pSkel = getScene().getCurvSkeleton();

        if(!m_pSkel) {
            throw std::runtime_error("SkelBDPTEG15Renderer - No skeleton for the scene");
        }

        if(!m_bUseSkelGridMapping) {
            computePrimarySkelMap();
        }
    }
}

void SkelBDPTEG15Renderer::beginFrame() {
    auto maxLightPathDepth = getMaxLightPathDepth();

    m_EmissionVertexBuffer.resize(m_nLightPathCount);
    m_LightPathBuffer.resize(maxLightPathDepth, m_nLightPathCount);

    m_nDistributionSize = getDistribution1DBufferSize(m_nLightPathCount);

    auto mis = [&](float v) {
        return Mis(v);
    };

    processTasksDeterminist(m_nLightPathCount, [&](uint32_t pathID, uint32_t threadID) {
        ThreadRNG rng(*this, threadID);
        auto pLightPath = m_LightPathBuffer.getSlicePtr(pathID);
        m_EmissionVertexBuffer[pathID] = sampleLightPath(pLightPath, maxLightPathDepth, getScene(),
                        m_LightSampler, getSppCount(), mis, rng);
    }, getThreadCount());

    // Partition light vertices according to framebuffer tiles
    ThreadRNG rng(*this, 0u);
    m_DirectImportanceSampleTilePartitionning.build(
                m_LightPathBuffer.size(),
                getScene(),
                getSensor(),
                getFramebufferSize(),
                getTileSize(),
                getTileCount2(),
                [&](std::size_t i) {
                    return m_LightPathBuffer[i].m_Intersection;
                },
                [&](std::size_t i) {
                    return m_LightPathBuffer[i].m_fPathPdf > 0.f;
                },
                rng);

    if(m_bUseSkeleton || !m_bUniformResampling) {
        computeLightVertexDistributions();
    }
}


void SkelBDPTEG15Renderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto spp = getSppCount();

    TileProcessingRenderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
        auto pixelID = getPixelIndex(x, y);

        for(auto i = 0u; i < getFramebufferChannelCount(); ++i) {
            accumulate(i, pixelID, Vec4f(0, 0, 0, 1));
        }

        for(auto sampleID = 0u; sampleID < spp; ++sampleID) {
            processSample(threadID, pixelID, sampleID, x, y);
        }
    });

    connectLightVerticesToCamera(threadID, tileID, viewport);
}

void SkelBDPTEG15Renderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                       uint32_t x, uint32_t y) const {
    auto maxEyePathDepth = getMaxEyePathDepth();
    auto maxLightPathDepth = getMaxLightPathDepth();

    auto mis = [&](float v) {
        return Mis(v);
    };
    ThreadRNG rng(*this, threadID);

    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto sensorSample = getFloat2(threadID);

    auto fImportanceScale = 1.f / getSppCount();
    BDPTPathVertex eyeVertex(getScene(), pixelSensor, sensorSample, pixelSample, m_nLightPathCount, mis);

    if(eyeVertex.m_Power == zero<Vec3f>() || !eyeVertex.m_fPathPdf) {
        return;
    }

    // Iterate over an eye path
    do {
        // Intersection with light source
        auto totalLength = eyeVertex.m_nDepth;
        if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
            auto contrib = fImportanceScale * computeEmittedRadiance(eyeVertex, getScene(), m_LightSampler, getSppCount(), mis);

            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
            accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
        }

        // Connections
        if(eyeVertex.m_Intersection) {
            Vec4i nodes;
            Vec4f nodeWeights;
            DiscreteDistribution4f nodeDist;

            if(m_bUseSkeleton) {
                if(eyeVertex.m_nDepth > 1u) {
                    nodes = Vec4i(m_pSkel->getNearestNode(eyeVertex.m_Intersection), -1, -1, -1);
                    nodeWeights = Vec4f(1, 0, 0, 0);
                } else {
                    skeletonMapping(eyeVertex.m_Intersection, pixelID, nodes, nodeWeights);

                    auto size = 0u;
                    Vec4f weights = zero<Vec4f>();
                    for (size = 0u;
                         size < min(m_nMaxNodeCount, 4u)
                         && nodes[size] >= 0;
                         ++size) {
                        weights[size] = nodeWeights[size];

                        if(eyeVertex.m_nDepth == 1u) {
                            accumulate(SELECTED_NODE0 + size, pixelID, Vec4f(getColor(nodes[size]), 0.f));
                        }
                    }

                    nodeDist = DiscreteDistribution4f(weights);
                }
            }

            // Connection with each light vertex
            for(auto j = 0u; j <= maxLightPathDepth; ++j) {
                auto lightPathDepth = j;
                auto totalLength = eyeVertex.m_nDepth + lightPathDepth + 1;

                if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                    auto pDistribution = m_DefaultLightVertexDistributionsArray.data() + lightPathDepth * m_nDistributionSize;

                    if(m_bUseSkeleton) {
                        Sample1u sampledNodeChannel;
                        if(eyeVertex.m_nDepth == 1u) {
                            sampledNodeChannel = nodeDist.sample(getFloat(threadID));
                        } else {
                            sampledNodeChannel = Sample1u(0, 1.f);
                        }

                        if(sampledNodeChannel.pdf > 0.f) {
                            auto sampledNodeIdx = nodes[sampledNodeChannel.value];
                            if(sampledNodeIdx >= 0) {
                                auto distribOffset = lightPathDepth * m_nDistributionSize;
                                pDistribution = m_SkelNodeLightVertexDistributionsArray.getSlicePtr(sampledNodeIdx) + distribOffset;
                            }
                        }
                    }

                    Sample1u lightPathDiscreteSample;

                    if(m_bUniformResampling) {
                        uint32_t idx = clamp(uint32_t(getFloat(threadID) * m_nLightPathCount), 0u, m_nLightPathCount - 1);
                        lightPathDiscreteSample = Sample1u(idx, 1.f / m_nLightPathCount);
                    } else {
                        lightPathDiscreteSample =
                            sampleDiscreteDistribution1D(pDistribution, m_nLightPathCount, getFloat(threadID));
                    }

                    if(lightPathDiscreteSample.pdf > 0.f) {
                        if(lightPathDepth == 0u) {
                            const auto& emissionVertex = m_EmissionVertexBuffer[lightPathDiscreteSample.value];

                            auto contrib = (1.f / (lightPathDiscreteSample.pdf * m_nLightPathCount)) * fImportanceScale * connectVertices(eyeVertex, emissionVertex, getScene(), getSppCount(), mis);

                            accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                            accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
                        } else {
                            auto firstPathOffset = lightPathDepth - 1;
                            auto lightPath = &m_LightPathBuffer[firstPathOffset + lightPathDiscreteSample.value * getMaxLightPathDepth()];

                            if(lightPath->m_fPathPdf > 0.f) {
                                auto contrib = (1.f / (lightPathDiscreteSample.pdf * m_nLightPathCount)) * fImportanceScale * connectVertices(eyeVertex, *lightPath, getScene(), getSppCount(), mis);

                                accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0));
                                accumulate(FINAL_RENDER_DEPTH1 + totalLength - 1u, pixelID, Vec4f(contrib, 0));
                            }
                        }
                    }
                }
            }
        }
    } while(eyeVertex.m_nDepth < maxEyePathDepth &&
            eyeVertex.extend(eyeVertex, getScene(), getSppCount(), false, rng, mis));
}

void SkelBDPTEG15Renderer::connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto rcpPathCount = 1.f / m_nLightPathCount;

    auto mis = [&](float v) {
        return Mis(v);
    };

    for(const auto& directImportanceSample: m_DirectImportanceSampleTilePartitionning[tileID]) {
        auto pLightVertex = &m_LightPathBuffer[directImportanceSample.m_nLightVertexIndex];

        auto totalDepth = 1 + pLightVertex->m_nDepth;
        if(!acceptPathDepth(totalDepth) || totalDepth > m_nMaxDepth) {
            continue;
        }

        auto pixel = directImportanceSample.m_Pixel;
        auto pixelID = BnZ::getPixelIndex(pixel, getFramebufferSize());

        PixelSensor sensor(getSensor(), pixel, getFramebufferSize());

        auto contrib = rcpPathCount * connectVertices(*pLightVertex,
                                       SensorVertex(&sensor, directImportanceSample.m_LensSample),
                                       getScene(), m_nLightPathCount, mis);

        accumulate(FINAL_RENDER, pixelID, Vec4f(contrib, 0.f));
        accumulate(FINAL_RENDER_DEPTH1 + totalDepth - 1u, pixelID, Vec4f(contrib, 0.f));
    }
}

void SkelBDPTEG15Renderer::computePrimarySkelMap() {
    auto framebufferSize = getFramebufferSize();
    m_SkelMappingData.resizeBuffers(framebufferSize.x * framebufferSize.y);

    auto pSkel = m_pSkel;

    processTasks(getPixelCount(), [&](uint32_t pixelID, uint32_t threadID) {
        auto pixel = getPixel(pixelID, getFramebufferSize());
        auto x = pixel.x;
        auto y = pixel.y;

        PixelSensor sensor(getSensor(), Vec2u(x, y), getFramebufferSize());

        RaySample raySample;
        Intersection I;

        sampleExitantRay(
                    sensor,
                    getScene(),
                    Vec2f(0.5f, 0.5f),
                    Vec2f(0.5f, 0.5f),
                    raySample,
                    I);

        if(I) {
            m_SkelMappingData.nodes[pixelID] = getNearestNodes(*pSkel, I.P, I.Ns, m_SkelMappingData.weights[pixelID],
                                [](uint32_t nodeIdx, Vec3f dir, float dist) { return false; });
        } else {
            m_SkelMappingData.nodes[pixelID] = Vec4i(-1);
        }
    }, getThreadCount());
}

void SkelBDPTEG15Renderer::computeLightVertexDistributions() {
    auto maxLightPathDepth = getMaxLightPathDepth();

    m_DefaultLightVertexDistributionsArray.resize((1 + maxLightPathDepth) * m_nDistributionSize);

    // Build the default distribution for each light sub-path depth
    buildDistribution1D([this](uint32_t pathIdx) {
        if(!m_EmissionVertexBuffer[pathIdx].m_pLight ||
                m_EmissionVertexBuffer[pathIdx].m_fLightPdf == 0.f) {
            return 0.f;
        }
        return 1.f;
    }, m_DefaultLightVertexDistributionsArray.data(), m_nLightPathCount);

    for(auto depth = 1u; depth <= maxLightPathDepth; ++depth) {
        auto distribOffset = depth * m_nDistributionSize;
        auto firstPathOffset = depth - 1;

        buildDistribution1D([this, firstPathOffset, maxLightPathDepth](uint32_t pathIdx) {
            auto vertexIdx = firstPathOffset + pathIdx * maxLightPathDepth;
            if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
                return 0.f;
            }
            return luminance(m_LightPathBuffer[vertexIdx].m_Power);
        }, m_DefaultLightVertexDistributionsArray.data() + distribOffset, m_nLightPathCount);
    }

    if(m_bUseSkeleton) {
        auto pSkel = m_pSkel;

        // A distribution contains all sub-paths of the same depth
        // There is maxLightPathDepth possible depth and m_nLightPathCount paths of a given depth
        m_SkelNodeLightVertexDistributionsArray.resize((1 + maxLightPathDepth) * m_nDistributionSize, pSkel->size());

        // For each node, compute intersections, irradiance values and distributions
        processTasksDeterminist(pSkel->size(), [&](uint32_t taskID, uint32_t threadID) {
            auto nodeIndex = taskID;
            auto nodePos = pSkel->getNode(nodeIndex).P;
            auto nodeRadius = pSkel->getNode(nodeIndex).maxball;

            // Build the distribution for emission vertices
            buildDistribution1D([&](uint32_t i) {
                auto pathIdx = i;

                if(!m_EmissionVertexBuffer[pathIdx].m_pLight ||
                        m_EmissionVertexBuffer[pathIdx].m_fLightPdf == 0.f) {
                    return 0.f;
                }

                RaySample shadowRaySample;
                auto L = m_EmissionVertexBuffer[pathIdx].m_pLight->sampleDirectIllumination(
                            getScene(),
                            getFloat2(threadID),
                            nodePos,
                            shadowRaySample);

                if(L == zero<Vec3f>() || shadowRaySample.pdf == 0.f) {
                    return 0.f;
                }

                L /= shadowRaySample.pdf;

                float weight = luminance(L);

                if(getScene().occluded(shadowRaySample.value)) {
                    return weight * m_fPowerSkelFactor;
                }

                return weight;
            }, m_SkelNodeLightVertexDistributionsArray.getSlicePtr(nodeIndex), m_nLightPathCount);

            // Build the distribution for scattering vertices
            for(auto depth = 1u; depth <= maxLightPathDepth; ++depth) {
                auto distribOffset = depth * m_nDistributionSize;
                auto firstPathOffset = depth - 1;
                auto visibilityOffset = nodeIndex * m_nLightPathCount * maxLightPathDepth;

                buildDistribution1D([this, visibilityOffset, firstPathOffset, maxLightPathDepth, nodePos, nodeRadius, nodeIndex](uint32_t i) {
                    auto vertexIdx = firstPathOffset + i * maxLightPathDepth;

                    if(m_LightPathBuffer[vertexIdx].m_fPathPdf == 0.f) {
                        return 0.f;
                    }

                    auto& I = m_LightPathBuffer[vertexIdx].m_Intersection;
                    auto dir = nodePos - I.P;
                    auto l = BnZ::length(dir);
                    dir /= l;

                    float weight = (1.f / sqr(l)) * luminance(m_LightPathBuffer[vertexIdx].m_Power);

                    if(dot(I.Ns, dir) <= 0.f || getScene().occluded(Ray(I, dir, l))) {
                        return weight * m_fPowerSkelFactor;
                    }

                    return weight;
                }, m_SkelNodeLightVertexDistributionsArray.getSlicePtr(nodeIndex) + distribOffset, m_nLightPathCount);
            }
        }, getThreadCount());
    }
}

void SkelBDPTEG15Renderer::skeletonMapping(const SurfacePoint& I, uint32_t pixelID,
                                           Vec4i& nearestNodes, Vec4f& weights) const {
    if(m_bUseSkelGridMapping) {
        nearestNodes = Vec4i(m_pSkel->getNearestNode(I), -1, -1, -1);
        weights = Vec4f(1, 0, 0, 0);
    } else {
        nearestNodes = m_SkelMappingData.nodes[pixelID];
        weights = m_SkelMappingData.weights[pixelID];
    }
}

void SkelBDPTEG15Renderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nLightPathCount));
    gui.addVarRW(BNZ_GUI_VAR(m_fPowerSkelFactor));
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxNodeCount));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseSkelGridMapping));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseSkeleton));
    gui.addVarRW(BNZ_GUI_VAR(m_bUniformResampling));
}

void SkelBDPTEG15Renderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
    serialize(xml, "visibilityPunishment", m_fPowerSkelFactor);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useSkelGridMapping", m_bUseSkelGridMapping);
    serialize(xml, "useSkeleton", m_bUseSkeleton);
    serialize(xml, "uniformResampling", m_bUniformResampling);
}

void SkelBDPTEG15Renderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "lightPathCount", m_nLightPathCount);
    serialize(xml, "visibilityPunishment", m_fPowerSkelFactor);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useSkelGridMapping", m_bUseSkelGridMapping);
    serialize(xml, "useSkeleton", m_bUseSkeleton);
    serialize(xml, "uniformResampling", m_bUniformResampling);
}

void SkelBDPTEG15Renderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("node0");
    addFramebufferChannel("node1");
    addFramebufferChannel("node2");
    addFramebufferChannel("node3");
    for(auto i : range(m_nMaxDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
