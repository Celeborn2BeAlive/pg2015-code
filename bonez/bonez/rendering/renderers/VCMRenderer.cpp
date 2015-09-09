#include "VCMRenderer.hpp"
#include <bonez/scene/lights/AreaLight.hpp>

#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

void VCMRenderer::preprocess() {
    m_LightSampler.initFrame(getScene());

    m_LightTraceOnly = false;
    m_UseVC = false;
    m_UseVM = false;
    m_Ppm = false;

    switch(m_AlgorithmType)
    {
    case kLightTrace:
        m_LightTraceOnly = true;
        break;
    case kPpm:
        m_Ppm   = true;
        m_UseVM = true;
        break;
    case kBpm:
        m_UseVM = true;
        break;
    case kBpt:
        m_UseVC = true;
        break;
    case kVcm:
        m_UseVC = true;
        m_UseVM = true;
        break;
    default:
        std::cerr << "Unknown algorithm requested" << std::endl;
        break;
    }

    Vec3f sceneCenter;
    boundingSphere(getScene().getBBox(), sceneCenter, m_SceneRadius);

    m_BaseRadius  = m_RadiusFactor * m_SceneRadius;

    m_nIterationCount = 0u;
}

void VCMRenderer::beginFrame() {
    m_nLightPathCount = getFramebuffer().getPixelCount() * getSppCount();
    m_nLightVertexCount = m_nLightPathCount * getMaxLightPathDepth();

    m_LightPathBuffer.resize(getMaxLightPathDepth(), m_nLightPathCount);

    auto maxLightPathDepth = getMaxLightPathDepth();

    // Sample light paths for each pixel
    TileProcessingRenderer::processTiles([&](uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
        TileProcessingRenderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
            auto pixelID = getPixelIndex(x, y);

            for(auto i = 0u, spp = getSppCount(); i < spp; ++i) {
                auto pLightPath = m_LightPathBuffer.getSlicePtr(pixelID * spp + i);
                sampleLightPath(getScene(),
                                threadID, pixelID, pLightPath,
                                maxLightPathDepth);
            }
        });
    });

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

    // For vertex merging:

    // Setup our radius, 1st iteration has aIteration == 0, thus offset
    float radius = m_BaseRadius;
    radius /= pow(float(m_nIterationCount + 1), 0.5f * (1 - m_RadiusAlpha));
    // Purely for numeric stability
    radius = max(radius, 1e-7f);
    const float radiusSqr = sqr(radius);

    // Factor used to normalise vertex merging contribution.
    // We divide the summed up energy by disk radius and number of light paths
    m_VmNormalization = 1.f / (radiusSqr * pi<float>() * m_nLightPathCount);

    // MIS weight constant [tech. rep. (20)], with n_VC = 1 and n_VM = mLightPathCount
    const float etaVCM = (pi<float>() * radiusSqr) * m_nLightPathCount;

    m_MisVmWeightFactor = m_UseVM ? Mis(etaVCM)       : 0.f;
    m_MisVcWeightFactor = m_UseVC ? Mis(1.f / etaVCM) : 0.f;

    if(m_UseVM)
    {
        m_LightVerticesHashGrid.Reserve(m_nLightPathCount);
        m_LightVerticesHashGrid.build(m_LightPathBuffer.data(), m_nLightVertexCount, radius);
    }

    ++m_nIterationCount;
}

void VCMRenderer::processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    auto spp = getSppCount();

    TileProcessingRenderer::processTilePixels(viewport, [&](uint32_t x, uint32_t y) {
        auto pixelID = getPixelIndex(x, y);

        for(auto i = 0u; i <= m_nMaxDepth; ++i) {
            accumulate(i, pixelID, Vec4f(0, 0, 0, 1));
        }

        for(auto sampleID = 0u; sampleID < spp; ++sampleID) {
            processSample(threadID, pixelID, sampleID, x, y);
        }
    });

    if(m_UseVC || m_LightTraceOnly) {
        connectLightVerticesToCamera(threadID, tileID, viewport);
    }
}

void VCMRenderer::processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                       uint32_t x, uint32_t y) const {
    PixelSensor pixelSensor(getSensor(), Vec2u(x, y), getFramebufferSize());

    auto pixelSample = getPixelSample(threadID, sampleID);
    auto sensorSample = getFloat2(threadID);

    auto fImportanceScale = 1.f / getSppCount();
    auto sourceVertex = BnZ::samplePrimaryEyeVertex<BidirPathVertex>(getScene(), pixelSensor,
                                              sensorSample, pixelSample);

    sourceVertex.power() *= fImportanceScale;

    if(sourceVertex.length() == 0u) {
        auto contrib = sourceVertex.power();
        // Hit on environment light, if exists
        accumulate(0, pixelID, Vec4f(contrib, 0));
        accumulate(1, pixelID, Vec4f(contrib, 0));
        return;
    }

    auto pLightPath = m_LightPathBuffer.data() + pixelID * getMaxLightPathDepth() * getSppCount() + sampleID * getMaxLightPathDepth();

    PathVertex previousEyeVertex;

    float lightPdf;
    auto pLight = m_LightSampler.sample(getScene(), getFloat(threadID), lightPdf);

    auto maxEyePathDepth = getMaxEyePathDepth();

    ThreadRNG rng(*this, threadID);
    for(const auto& vertex: makePath(getScene(), sourceVertex, rng)) {
        auto eyeVertexIndex = vertex.length() - 1u; // index of vertex in pEyePath;

        PathVertex eyePathVertex;
        eyePathVertex.init(vertex);

        auto g = vertex.previousPointToIncidentDirectionJacobian();

        if(eyeVertexIndex == 0u) {
            eyePathVertex.m_fdVCM = Mis(m_nLightPathCount / vertex.pdfWrtArea());
            eyePathVertex.m_fdVC = 0.f;
            eyePathVertex.m_fdVM = 0.f;
        } else {
            if((vertex.sampledEvent() & ScatteringEvent::Specular) && previousEyeVertex.m_BSDF.isDelta()) {
                eyePathVertex.m_fdVCM = 0.f;
                eyePathVertex.m_fdVC = Mis(g) * previousEyeVertex.m_fdVC;
                eyePathVertex.m_fdVM = Mis(g) * previousEyeVertex.m_fdVM;
            } else {
                float reversePdf = previousEyeVertex.m_BSDF.pdf(-vertex.bsdf().getIncidentDirection(), true);

                eyePathVertex.m_fdVCM = Mis(getSppCount() / eyePathVertex.m_fPdfWrtArea);
                eyePathVertex.m_fdVC = Mis(g / eyePathVertex.m_fPdfWrtArea) *
                        (m_MisVmWeightFactor + previousEyeVertex.m_fdVCM + Mis(reversePdf) * previousEyeVertex.m_fdVC);
                eyePathVertex.m_fdVM = Mis(g / eyePathVertex.m_fPdfWrtArea) *
                        (1.f + previousEyeVertex.m_fdVCM * m_MisVcWeightFactor + Mis(reversePdf) * previousEyeVertex.m_fdVM);
            }
        }
        previousEyeVertex = eyePathVertex;

        // PPM merges only at the first non-specular surface from camera
        if(eyeVertexIndex > 0u && m_Ppm && !(eyePathVertex.m_nScatteringEvent & ScatteringEvent::Specular)) {
            break;
        }

        { // Intersection with light source
            auto totalLength = eyePathVertex.m_nDepth;
            if(acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
                computeEmittedRadiance(threadID, pixelID, sampleID, x, y, eyePathVertex);
                if(m_LightTraceOnly) {
                    break;
                }
            }
        }

        if(m_UseVC && !eyePathVertex.m_BSDF.isDelta()) {
            vertexConnection(threadID, pixelID, sampleID, x, y, pLight, lightPdf, pLightPath, eyePathVertex);
        }

        // Vertex merging
        if(m_UseVM && !eyePathVertex.m_BSDF.isDelta()) {
            vertexMerging(threadID, pixelID, sampleID, eyePathVertex);
        }

        if(vertex.length() == maxEyePathDepth) {
            break;
        }
    }
}

void VCMRenderer::sampleLightPath(
        const Scene& scene,
        uint32_t threadID, uint32_t pixelID, PathVertex* pLightPath,
        uint32_t maxLightPathDepth) const {
    std::for_each(pLightPath, pLightPath + maxLightPathDepth, [&](PathVertex& v) { v.m_fPathPdf = 0.f; });

    if(maxLightPathDepth > 0u) {
        ThreadRNG rng(*this, threadID);
        for(const auto& vertex: makeLightPath(getScene(), m_LightSampler, rng)) {
            auto idx = vertex.length() - 1u;

            pLightPath[idx].init(vertex);

            auto g = vertex.previousPointToIncidentDirectionJacobian();

            if(idx > 0u) {
                const auto& previousVertex = pLightPath[idx - 1];

                if((vertex.sampledEvent() & ScatteringEvent::Specular) && previousVertex.m_BSDF.isDelta()) {
                    pLightPath[idx].m_fdVCM = 0.f;
                    pLightPath[idx].m_fdVC = Mis(g) * previousVertex.m_fdVC;
                    pLightPath[idx].m_fdVM = Mis(g) * previousVertex.m_fdVM;
                } else {
                    float reversePdf = previousVertex.m_BSDF.pdf(-vertex.bsdf().getIncidentDirection(), true);

                    pLightPath[idx].m_fdVCM = Mis(getSppCount() / pLightPath[idx].m_fPdfWrtArea);
                    pLightPath[idx].m_fdVC = Mis(g / pLightPath[idx].m_fPdfWrtArea) *
                            (m_MisVmWeightFactor + previousVertex.m_fdVCM + Mis(reversePdf) * previousVertex.m_fdVC);
                    pLightPath[idx].m_fdVM = Mis(g / pLightPath[idx].m_fPdfWrtArea) *
                            (1.f + previousVertex.m_fdVCM * m_MisVcWeightFactor + Mis(reversePdf) * previousVertex.m_fdVM);
                }
            } else {
                pLightPath[idx].m_fdVCM = Mis(getSppCount() / pLightPath[idx].m_fPdfWrtArea);
                pLightPath[idx].m_fdVC = Mis(g / pLightPath[idx].m_fPathPdf);
                pLightPath[idx].m_fdVC = pLightPath[idx].m_fdVC * m_MisVcWeightFactor;
            }

            if(vertex.length() == maxLightPathDepth) {
                break;
            }
        }
    }
}

void VCMRenderer::computeEmittedRadiance(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                                                              uint32_t x, uint32_t y, const PathVertex& eyeVertex) const {
    if(eyeVertex.m_Intersection.Le == zero<Vec3f>()) {
        return;
    }

    auto contrib = eyeVertex.m_Power * eyeVertex.m_Intersection.Le;

    auto rcpWeight = 1.f;
    if(eyeVertex.m_nDepth > 1u) {
        if(!eyeVertex.m_Intersection) {
            // Hit on environment map (if present) should be handled here

        } else {
            auto meshID = eyeVertex.m_Intersection.meshID;
            auto lightID = getScene().getGeometry().getMesh(meshID).m_nLightID;

            const auto& pLight = getScene().getLightContainer().getLight(lightID);
            const auto* pAreaLight = static_cast<const AreaLight*>(pLight.get());

            float pointPdfWrtArea, directionPdfWrtSolidAngle;
            pAreaLight->pdf(eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection(), getScene(),
                            pointPdfWrtArea, directionPdfWrtSolidAngle);

            pointPdfWrtArea *= m_LightSampler.pdf(lightID);

            rcpWeight += Mis(pointPdfWrtArea / getSppCount()) * (eyeVertex.m_fdVCM + Mis(directionPdfWrtSolidAngle) * eyeVertex.m_fdVC);
        }
    }

    auto weight = 1.f / rcpWeight;

    auto weightedContrib = weight * contrib;
    accumulate(0, pixelID, Vec4f(weightedContrib, 0));
    accumulate(eyeVertex.m_nDepth, pixelID, Vec4f(weightedContrib, 0));
}

void VCMRenderer::computeDirectIllumination(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                               uint32_t x, uint32_t y, const PathVertex& eyeVertex,
                               const Light& light, float lightPdf) const {
    float sampledPointPdfWrtArea, sampledPointToIncidentDirectionJacobian, revPdfWrtArea;
    RaySample shadowRay;
    auto positionSample = getFloat2(threadID);
    auto Le = light.sampleDirectIllumination(getScene(), positionSample, eyeVertex.m_Intersection, shadowRay, sampledPointToIncidentDirectionJacobian,
                                               sampledPointPdfWrtArea, revPdfWrtArea);

    if(Le == zero<Vec3f>() || shadowRay.pdf == 0.f) {
        return;
    }

    shadowRay.pdf *= lightPdf;
    sampledPointPdfWrtArea *= lightPdf;

    Le /= shadowRay.pdf;

    float cosAtEyeVertex;
    float eyeDirPdf, eyeRevPdf;
    auto fr = eyeVertex.m_BSDF.eval(shadowRay.value.dir, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

    if(fr == zero<Vec3f>() || getScene().occluded(shadowRay.value)) {
        return;
    }

    auto contrib = eyeVertex.m_Power * Le * fr * cosAtEyeVertex;

    auto rcpWeight = 1.f;

    {
       auto pdfWrtArea = eyeDirPdf * sampledPointToIncidentDirectionJacobian;
       rcpWeight += Mis(pdfWrtArea / (getSppCount() * sampledPointPdfWrtArea));
    }

    rcpWeight += Mis(revPdfWrtArea / getSppCount()) * (m_MisVmWeightFactor + eyeVertex.m_fdVCM + Mis(eyeRevPdf) * eyeVertex.m_fdVC);

    auto weight = 1.f / rcpWeight;

    auto weightedContrib = weight * contrib;
    accumulate(0, pixelID, Vec4f(weightedContrib, 0));
    accumulate(eyeVertex.m_nDepth + 1, pixelID, Vec4f(weightedContrib, 0));
}

void VCMRenderer::connectVertices(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                                                       uint32_t x, uint32_t y, const PathVertex& eyeVertex,
                                                       const PathVertex& lightVertex) const {
    Vec3f incidentDirection;
    float dist;
    auto G = geometricFactor(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);
    Ray incidentRay(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);

    float cosAtLightVertex, cosAtEyeVertex;
    float lightDirPdf, lightRevPdf;
    float eyeDirPdf, eyeRevPdf;

    auto M = lightVertex.m_BSDF.eval(-incidentDirection, cosAtLightVertex, &lightDirPdf, &lightRevPdf) *
            eyeVertex.m_BSDF.eval(incidentDirection, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

    if(G > 0.f && M != zero<Vec3f>()) {
        bool isVisible = !getScene().occluded(incidentRay);

        if(isVisible) {
            auto rcpWeight = 1.f;

            {
                auto pdfWrtArea = eyeDirPdf * abs(cosAtLightVertex) / sqr(dist);
                rcpWeight += Mis(pdfWrtArea / getSppCount()) * (m_MisVmWeightFactor + lightVertex.m_fdVCM + Mis(lightRevPdf) * lightVertex.m_fdVC);
            }

            {
                auto pdfWrtArea = lightDirPdf * abs(cosAtEyeVertex) / sqr(dist);
                rcpWeight += Mis(pdfWrtArea / getSppCount()) * (m_MisVmWeightFactor + eyeVertex.m_fdVCM + Mis(eyeRevPdf) * eyeVertex.m_fdVC);
            }

            auto weight = 1.f / rcpWeight;

            auto contrib = eyeVertex.m_Power * lightVertex.m_Power * G * M;

            auto weightedContrib = weight * contrib;
            accumulate(0, pixelID, Vec4f(weightedContrib, 0));
            accumulate(eyeVertex.m_nDepth + 1 + lightVertex.m_nDepth, pixelID, Vec4f(weightedContrib, 0));
        }
    }
}

void VCMRenderer::vertexConnection(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, uint32_t x, uint32_t y,
                                const Light* pLight, float lightPdf, const PathVertex* pLightPath,
                                const PathVertex& eyeVertex) const {
    { // Direct illumination
        auto totalLength = eyeVertex.m_nDepth + 1;
        if(pLight && lightPdf && acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
            computeDirectIllumination(threadID, pixelID, sampleID, x, y, eyeVertex, *pLight, lightPdf);
        }
    }

    const auto maxLightPathDepth = getMaxLightPathDepth();
    // Connection with each light vertex
    for(auto j = 0u; j < maxLightPathDepth; ++j) {
        auto lightPath = pLightPath + j;
        auto totalLength = eyeVertex.m_nDepth + lightPath->m_nDepth + 1;

        if(lightPath->m_fPathPdf > 0.f && acceptPathDepth(totalLength) && totalLength <= m_nMaxDepth) {
            connectVertices(threadID, pixelID, sampleID, x, y, eyeVertex, *lightPath);
        }
    }
}

void VCMRenderer::vertexMerging(uint32_t threadID, uint32_t pixelID, uint32_t sampleID, const PathVertex& eyeVertex) const {
    auto vmContrib = zero<Vec3f>();
    auto query = [&](const PathVertex& lightPathVertex) {
        // Reject if full path length below/above min/max path length
        auto totalDepth = lightPathVertex.m_nDepth + eyeVertex.m_nDepth;
        if(!acceptPathDepth(totalDepth) || totalDepth > m_nMaxDepth) {
             return;
        }

        // Retrieve light incoming direction in world coordinates
        const Vec3f lightDirection = lightPathVertex.m_BSDF.getIncidentDirection();

//        if(dot(lightDirection, pEyePath[k].incidentDirection) < 0.f) {
//            return;
//        }

        if(dot(lightDirection, eyeVertex.m_Intersection.Ns) < 0.f) {
            return;
        }

        const Vec3f fr = eyeVertex.m_BSDF.eval(lightDirection);

        auto brdfDirPdf = eyeVertex.m_BSDF.pdf(lightDirection);
        auto brdfRevPdf = eyeVertex.m_BSDF.pdf(lightDirection, true);

        if(fr == zero<Vec3f>())
            return;

        // Partial light sub-path MIS weight [tech. rep. (38)]
        const float wLight = lightPathVertex.m_fdVCM * m_MisVcWeightFactor +
            lightPathVertex.m_fdVM * Mis(brdfDirPdf);

        // Partial eye sub-path MIS weight [tech. rep. (39)]
        const float wCamera = eyeVertex.m_fdVCM * m_MisVcWeightFactor +
            eyeVertex.m_fdVM * Mis(brdfRevPdf);

        // Full path MIS weight [tech. rep. (37)]. No MIS for PPM
        const float misWeight = m_Ppm ?
            1.f :
            1.f / (wLight + 1.f + wCamera);

        auto contrib = misWeight * fr * lightPathVertex.m_Power;
        vmContrib += contrib;

        accumulate(totalDepth, pixelID, Vec4f(eyeVertex.m_Power * m_VmNormalization * contrib, 0.f));
    };

    m_LightVerticesHashGrid.process(m_LightPathBuffer.data(), getPosition(eyeVertex), query);
    accumulate(0u, pixelID, Vec4f(eyeVertex.m_Power * m_VmNormalization * vmContrib, 0.f));
}

void VCMRenderer::connectLightVerticesToCamera(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const {
    const auto& scene = getScene();

    auto rcpPathCount = 1.f / m_nLightPathCount;

    for(const auto& directImportanceSample: m_DirectImportanceSampleTilePartitionning[tileID]) {
        auto pLightVertex = &m_LightPathBuffer[directImportanceSample.m_nLightVertexIndex];

        auto totalDepth = 1 + pLightVertex->m_nDepth;
        if(!acceptPathDepth(totalDepth) || totalDepth > m_nMaxDepth) {
            continue;
        }

        auto pixel = directImportanceSample.m_Pixel;
        auto pixelID = BnZ::getPixelIndex(pixel, getFramebufferSize());

        PixelSensor sensor(getSensor(), pixel, getFramebufferSize());

        RaySample shadowRaySample;
        float revPdfWrtArea;
        auto We = sensor.sampleDirectImportance(getScene(), directImportanceSample.m_LensSample, pLightVertex->m_Intersection,
                                                shadowRaySample, nullptr, nullptr, nullptr,
                                                &revPdfWrtArea, nullptr);

        We /= shadowRaySample.pdf;

        float cosThetaOutDir;
        float bsdfRevPdfW;
        auto fr = pLightVertex->m_BSDF.eval(shadowRaySample.value.dir, cosThetaOutDir, nullptr, &bsdfRevPdfW);

        const float wLight = Mis(revPdfWrtArea * rcpPathCount) *
                (m_MisVmWeightFactor + pLightVertex->m_fdVCM + Mis(bsdfRevPdfW) * pLightVertex->m_fdVC);

        const float misWeight = m_LightTraceOnly ? 1.f : (1.f / (wLight + 1.f));

        if(fr != zero<Vec3f>()) {
            if(!scene.occluded(shadowRaySample.value)) {
                accumulate(0u, pixelID, Vec4f(misWeight * rcpPathCount * pLightVertex->m_Power * fr * cosThetaOutDir * We, 0.f));
                accumulate(totalDepth, pixelID, Vec4f(misWeight * rcpPathCount * pLightVertex->m_Power * fr * cosThetaOutDir * We, 0.f));
            }
        }
    }
}

uint32_t VCMRenderer::getMaxLightPathDepth() const {
    return m_nMaxDepth - 1;
}

uint32_t VCMRenderer::getMaxEyePathDepth() const {
    return m_nMaxDepth;
}

std::string VCMRenderer::getAlgorithmType() const {
    const std::string mapping[] = {
        "lt", "ppm", "bpm", "bpt", "vcm"
    };
    return mapping[m_AlgorithmType];
}

void VCMRenderer::setAlgorithmType(const std::string& algorithmType) {
    const std::string mapping[] = {
        "lt", "ppm", "bpm", "bpt", "vcm"
    };
    auto it = std::find(std::begin(mapping), std::end(mapping), algorithmType);
    if(it == std::end(mapping)) {
        std::cerr << "Unrecognized algorithm type for VCMRenderer" << std::endl;
        m_AlgorithmType = kVcm;
        return;
    }
    m_AlgorithmType = AlgorithmType(it - std::begin(mapping));
}

void VCMRenderer::doExposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nMaxDepth));
    gui.addVarRW(BNZ_GUI_VAR(m_RadiusFactor));
    gui.addVarRW(BNZ_GUI_VAR(m_RadiusAlpha));
    const char* algorithms[] = { "kLightTrace", "kPpm", "kBpm", "kBpt", "kVcm" };
    gui.addRadioButtons("algorithm", m_AlgorithmType, 5, algorithms);
}

void VCMRenderer::doLoadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "maxDepth", m_nMaxDepth);
    std::string algorithmType;
    serialize(xml, "algorithmType", algorithmType);
    setAlgorithmType(algorithmType);
    serialize(xml, "radiusFactor", m_RadiusFactor);
    serialize(xml, "radiusAlpha", m_RadiusAlpha);
}

void VCMRenderer::doStoreSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "maxDepth", m_nMaxDepth);
    serialize(xml, "algorithmType", getAlgorithmType());
    serialize(xml, "radiusFactor", m_RadiusFactor);
    serialize(xml, "radiusAlpha", m_RadiusAlpha);
}

void VCMRenderer::initFramebuffer() {
    addFramebufferChannel("final_render");
    for(auto i : range(m_nMaxDepth)) {
        addFramebufferChannel("depth_" + toString(i + 1));
    }
}

}
