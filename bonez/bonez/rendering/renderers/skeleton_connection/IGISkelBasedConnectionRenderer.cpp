#include "IGISkelBasedConnectionRenderer.hpp"
#include "../paths.hpp"
#include "skel_utils.hpp"

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void IGISkelBasedConnectionRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());

    if(m_bUseSkeleton) {
        m_pSkel = getScene().getCurvSkeleton();

        if(!m_pSkel) {
            throw std::runtime_error("IGISkelBasedConnectionRenderer - No skeleton for the scene");
        }

        if(!m_bUseSkelGridMapping) {
            computeSkeletonMapping();
        }
    }
}

uint32_t IGISkelBasedConnectionRenderer::getMaxLightPathDepth() const {
    return m_nMaxPathDepth - 2;
}

void IGISkelBasedConnectionRenderer::beginFrame() {
    // Sample all VPLs, allocs buffers

    if(m_nPathCount > 0u && m_nMaxPathDepth > 2u) {
        auto maxLightPathDepth = getMaxLightPathDepth();
        m_SurfaceVPLBuffer.resize(getMaxLightPathDepth() * m_nPathCount);

        float scaleCoeff = 1.f / m_nPathCount;

        for(auto i = 0u; i < m_nPathCount; ++i) {
            auto pLightPath = m_SurfaceVPLBuffer.data() + i * maxLightPathDepth;
            std::for_each(pLightPath, pLightPath + maxLightPathDepth, [&](SurfaceVPL& v) { v.pdf = 0.f; });

            ThreadRNG rng(*this, 0u);
            for(const auto& vertex: makeLightPath(getScene(), m_LightSampler, rng)) {
                pLightPath->init(vertex);
                pLightPath->power *= scaleCoeff;

                if(vertex.length() == maxLightPathDepth) {
                    break;
                }
                ++pLightPath;
            }
        }

        m_nVPLCount = maxLightPathDepth * m_nPathCount;
        m_nDistributionSize = getDistribution1DBufferSize(m_nVPLCount);

        if(m_bUseSkeleton || !m_bUniformResampling) {
            computeSkelNodeVPLDistributions();
        }
    }
}

void IGISkelBasedConnectionRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y) const {
    for(auto i = 0u; i <= getFramebufferChannelCount(); ++i) {
        accumulate(i, pixelID, Vec4f(0.f, 0.f, 0.f, 1.f));
    }

    PixelSensor sensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto lensSample = getFloat2(threadID);

    RaySample raySample;
    Intersection I;

    sampleExitantRay(
                sensor,
                getScene(),
                lensSample,
                pixelSample,
                raySample,
                I);

    auto ray = raySample.value;

    Vec3f L = Vec3f(0.f);
    Vec3f wo = -ray.dir;
    auto currentDepth = 1u;
    Vec3f throughput(1.f);

    if(!I) {
        L += I.Le;
        accumulate(FINAL_RENDER_DEPTH1, pixelID, Vec4f(I.Le, 0.f));
    } else {
        auto bsdf = BSDF(wo, I, getScene());

        if(m_nMaxPathDepth >= 1u && acceptPathDepth(1u)) {
            L += I.Le;
            accumulate(FINAL_RENDER_DEPTH1, pixelID, Vec4f(I.Le, 0.f));
        }

        // Direct illumination
        if(1 + currentDepth <= m_nMaxPathDepth && acceptPathDepth(1 + currentDepth)) {
            float lightPdf;
            auto pLight = m_LightSampler.sample(getScene(), getFloat(threadID), lightPdf);
            if(pLight && lightPdf) {
                RaySample shadowRay;
                auto Le = pLight->sampleDirectIllumination(getScene(), getFloat2(threadID), I, shadowRay);

                if(Le != zero<Vec3f>() && shadowRay.pdf) {
                    float cosThetaOutDir;
                    auto fr = bsdf.eval(shadowRay.value.dir, cosThetaOutDir);
                    auto contrib = Le * fr * abs(cosThetaOutDir) / shadowRay.pdf;
                    if(contrib != zero<Vec3f>() && !getScene().occluded(shadowRay.value)) {
                        L += contrib;
                        accumulate(FINAL_RENDER_DEPTH1 + currentDepth, pixelID, Vec4f(contrib, 0.f));
                    }
                }
            }
        }

        if(m_bUseSkeleton) {
            Vec4i nodes;
            Vec4f nodeWeights;

            skeletonMapping(I, pixelID, nodes, nodeWeights);

            auto size = 0u;
            Vec4f weights = zero<Vec4f>();
            for (size = 0u;
                 size < min(m_nMaxNodeCount, 4u)
                 && nodes[size] >= 0;
                 ++size) {
                weights[size] = nodeWeights[size];

                accumulate(SELECTED_NODE0 + size, pixelID, Vec4f(getColor(nodes[size]), 0.f));
            }

            auto nodeDist = DiscreteDistribution4f(weights);
            auto sampledNodeChannel = nodeDist.sample(getFloat(threadID));

            auto pDistribution = m_DefaultVPLDistributionsArray.data();

            if(sampledNodeChannel.pdf > 0.f) {
                auto sampledNodeIdx = nodes[sampledNodeChannel.value];
                if(sampledNodeIdx >= 0) {
                    pDistribution = m_SkelNodeVPLDistributionsArray.getSlicePtr(sampledNodeIdx);
                }
            }

            auto vplSample = [&]() {
                if(m_bUniformResampling) {
                    uint32_t idx = clamp(uint32_t(getFloat(threadID) * m_nVPLCount), 0u, m_nVPLCount - 1);
                    return Sample1u(idx, 1.f / m_nVPLCount);
                }
                return sampleDiscreteDistribution1D(pDistribution, m_nVPLCount, getFloat(threadID));
            }();

            if(vplSample.pdf > 0.f) {
                const auto& vpl = m_SurfaceVPLBuffer[vplSample.value];

                if(vpl.pdf) {
                    auto pathDepth = vpl.depth + 1 + currentDepth;

                    if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                        auto contrib = evalVPLContribution(vpl, I, bsdf) / vplSample.pdf;
                        L += contrib;
                        accumulate(FINAL_RENDER_DEPTH1 + pathDepth - 1u, pixelID, Vec4f(contrib, 0.f));
                    }
                }
            }

        } else {
            if(m_bUniformResampling) {
                auto idx = clamp(uint32_t(getFloat(threadID) * m_nVPLCount), 0u, m_nVPLCount - 1);
                auto vplSample = Sample1u(idx, 1.f / m_nVPLCount);
                const auto& vpl = m_SurfaceVPLBuffer[vplSample.value];
                if(vpl.pdf) {
                    auto pathDepth = vpl.depth + 1 + currentDepth;

                    if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                        auto contrib = evalVPLContribution(vpl, I, bsdf) / vplSample.pdf;
                        L += contrib;
                        accumulate(FINAL_RENDER_DEPTH1 + pathDepth - 1u, pixelID, Vec4f(contrib, 0.f));
                    }
                }
            } else {
                // VPL illumination
                for(const auto& vpl: m_SurfaceVPLBuffer) {
                    if(vpl.pdf) {
                        auto pathDepth = vpl.depth + 1 + currentDepth;

                        if(pathDepth <= m_nMaxPathDepth && acceptPathDepth(pathDepth)) {
                            auto contrib = evalVPLContribution(vpl, I, bsdf);
                            L += contrib;
                            accumulate(FINAL_RENDER_DEPTH1 + pathDepth - 1u, pixelID, Vec4f(contrib, 0.f));
                        }
                    }
                }
            }
        }
    }

    accumulate(FINAL_RENDER, pixelID, Vec4f(L, 0.f));
}

Vec3f IGISkelBasedConnectionRenderer::evalVPLContribution(const SurfaceVPL& vpl, const Intersection& I, const BSDF& bsdf) const {
    Vec3f wi;
    float dist;
    auto G = geometricFactor(I, vpl.lastVertex, wi, dist);
    if(G > 0.f) {
        float cosThetaOutVPL, costThetaOutBSDF;
        auto M = bsdf.eval(wi, costThetaOutBSDF) * vpl.lastVertexBSDF.eval(-wi, cosThetaOutVPL);
        if(M != zero<Vec3f>()) {
            Ray shadowRay(I, vpl.lastVertex, wi, dist);
            if(!getScene().occluded(shadowRay)) {
                return M * G * vpl.power;
            }
        }
    }
    return zero<Vec3f>();
}

void IGISkelBasedConnectionRenderer::computeSkelNodeVPLDistributions() {
    m_DefaultVPLDistributionsArray.resize(m_nDistributionSize);

    buildDistribution1D([&](uint32_t i) {
        if(m_SurfaceVPLBuffer[i].pdf == 0.f) {
            return 0.f;
        }
        return luminance(m_SurfaceVPLBuffer[i].power);
    }, m_DefaultVPLDistributionsArray.data(), m_nVPLCount);

    if(m_bUseSkeleton) {
        auto pSkel = m_pSkel;

        m_SkelNodeVPLDistributionsArray.resize(m_nDistributionSize, pSkel->size());

        // For each node, compute intersections, irradiance values and distributions
        processTasksDeterminist(pSkel->size(), [&](uint32_t taskID, uint32_t threadID) {
            auto nodeIndex = taskID;
            auto nodePos = pSkel->getNode(nodeIndex).P;
            auto nodeRadius = pSkel->getNode(nodeIndex).maxball;

            buildDistribution1D([&](uint32_t i) {
                if(m_SurfaceVPLBuffer[i].pdf == 0.f) {
                    return 0.f;
                }

                auto& I = m_SurfaceVPLBuffer[i].lastVertex;
                auto dir = nodePos - I.P;
                auto l = BnZ::length(dir);
                dir /= l;

                float weight = sqr(nodeRadius) * (1.f / sqr(l)) * luminance(m_SurfaceVPLBuffer[i].power);

                if(dot(I.Ns, dir) <= 0.f || getScene().occluded(Ray(I, dir, l))) {
                    return weight * m_fPowerSkelFactor;
                }

                return weight;
            }, m_SkelNodeVPLDistributionsArray.getSlicePtr(nodeIndex), m_nVPLCount);
        }, getThreadCount());
    }
}

void IGISkelBasedConnectionRenderer::computeSkeletonMapping() {
    auto framebufferSize = getFramebufferSize();
    m_SkelMappingData.resizeBuffers(framebufferSize.x * framebufferSize.y);

    Vec2u tileCount = framebufferSize / getTileSize() +
            Vec2u(framebufferSize % getTileSize() != zero<Vec2u>());

    auto totalCount = tileCount.x * tileCount.y;

    auto pSkel = m_pSkel;

    auto task = [&](uint32_t threadID) {
        auto loopID = 0u;
        while(true) {
            auto tileID = loopID * getThreadCount() + threadID;
            ++loopID;

            if(tileID >= totalCount) {
                return;
            }

            uint32_t tileX = tileID % tileCount.x;
            uint32_t tileY = tileID / tileCount.x;

            Vec2u tileOrg = Vec2u(tileX, tileY) * getTileSize();
            auto viewport = Vec4u(tileOrg, getTileSize());

            if(viewport.x + viewport.z > framebufferSize.x) {
                viewport.z = framebufferSize.x - viewport.x;
            }

            if(viewport.y + viewport.w > framebufferSize.y) {
                viewport.w = framebufferSize.y - viewport.y;
            }

            auto xEnd = viewport.x + viewport.z;
            auto yEnd = viewport.y + viewport.w;

            for(auto y = viewport.y; y < yEnd; ++y) {
                for(auto x = viewport.x; x < xEnd; ++x) {
                    auto pixelIdx = getPixelIndex(x, y);

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
                        m_SkelMappingData.nodes[pixelIdx] = getNearestNodes(*pSkel, I.P, I.Ns, m_SkelMappingData.weights[pixelIdx],
                                            [](uint32_t nodeIdx, Vec3f dir, float dist) { return false; });
                    } else {
                        m_SkelMappingData.nodes[pixelIdx] = Vec4i(-1);
                    }
                }
            }
        }
    };

    launchThreads(task, getThreadCount());
}

void IGISkelBasedConnectionRenderer::skeletonMapping(const SurfacePoint& I, uint32_t pixelID,
                                           Vec4i& nearestNodes, Vec4f& weights) const {
    if(m_bUseSkelGridMapping) {
        nearestNodes = Vec4i(m_pSkel->getNearestNode(I), -1, -1, -1);
        weights = Vec4f(1, 0, 0, 0);
    } else {
        nearestNodes = m_SkelMappingData.nodes[pixelID];
        weights = m_SkelMappingData.weights[pixelID];
    }
}

uint32_t IGISkelBasedConnectionRenderer::getVPLCountPerNode() const {
    return m_nPathCount * getMaxLightPathDepth();
}

void IGISkelBasedConnectionRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxPathDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_nPathCount));
    gui.addVarRW(BNZ_GUI_VAR(m_fPowerSkelFactor));
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxNodeCount));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseSkelGridMapping));
    gui.addVarRW(BNZ_GUI_VAR(m_bUseSkeleton));
    gui.addVarRW(BNZ_GUI_VAR(m_bUniformResampling));
}

void IGISkelBasedConnectionRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
    serialize(xml, "visibilityPunishment", m_fPowerSkelFactor);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useSkelGridMapping", m_bUseSkelGridMapping);
    serialize(xml, "useSkeleton", m_bUseSkeleton);
    serialize(xml, "uniformResampling", m_bUniformResampling);
}

void IGISkelBasedConnectionRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxPathDepth);
    serialize(xml, "pathCount", m_nPathCount);
    serialize(xml, "visibilityPunishment", m_fPowerSkelFactor);
    serialize(xml, "maxNodeCount", m_nMaxNodeCount);
    serialize(xml, "useSkelGridMapping", m_bUseSkelGridMapping);
    serialize(xml, "useSkeleton", m_bUseSkeleton);
    serialize(xml, "uniformResampling", m_bUniformResampling);
}

void IGISkelBasedConnectionRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
        processSample(threadID, pixelID, sampleID, x, y);
    });
}

void IGISkelBasedConnectionRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    addFramebufferChannel("node0");
    addFramebufferChannel("node1");
    addFramebufferChannel("node2");
    addFramebufferChannel("node3");
    for(auto i : range(m_nMaxPathDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
