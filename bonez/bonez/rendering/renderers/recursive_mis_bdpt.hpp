#pragma once

#include <bonez/scene/Scene.hpp>
#include <bonez/scene/shading/BSDF.hpp>

#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/lights/Light.hpp>
#include <bonez/scene/lights/AreaLight.hpp>

#include <bonez/scene/sensors/Sensor.hpp>
#include <bonez/scene/sensors/PixelSensor.hpp>

#include "DirectImportanceSampleTilePartionning.hpp"

namespace BnZ {

struct EmissionVertex {
    const Light* m_pLight;
    float m_fLightPdf;
    Vec2f m_PositionSample;

    EmissionVertex() = default;

    EmissionVertex(const Light* pLight, float lightPdf, const Vec2f& positionSample):
        m_pLight(pLight), m_fLightPdf(lightPdf), m_PositionSample(positionSample) {
    }
};

struct SensorVertex {
    const Sensor* m_pSensor;
    Vec2f m_PositionSample;

    SensorVertex() = default;

    SensorVertex(const Sensor* pSensor, const Vec2f& positionSample):
        m_pSensor(pSensor), m_PositionSample(positionSample) {
    }
};

class BDPTPathVertex {
public:
    Intersection m_Intersection;
    BSDF m_BSDF;
    float m_fPathPdf; // pdf of the complete path up to that vertex
    float m_fPdfWrtArea; // pdf of the last vertex, conditional to the previous
    Vec3f m_Power; // Monte-Carlo estimate of the power/importance carried by the complete path
    uint32_t m_nDepth; // number of edges of the path

    float m_fdVCM;
    float m_fdVC;

    BDPTPathVertex() = default;

    // Create a primary eve vertex
    template<typename MisFunctor>
    BDPTPathVertex(const Scene& scene,
                   const Sensor& sensor,
                   const Vec2f& lensSample,
                   const Vec2f& imageSample,
                   uint32_t pathCount,
                   MisFunctor&& mis) {
        RaySample raySample;
        float rayOriginPdf, rayDirectionPdf, intersectionPdfWrtArea,
                rayOriginToIncidentDirJacobian;
        auto We = sensor.sampleExitantRay(scene, lensSample, imageSample, raySample, rayOriginPdf, rayDirectionPdf, m_Intersection,
                                          rayOriginToIncidentDirJacobian, intersectionPdfWrtArea);

        if(We == zero<Vec3f>() || raySample.pdf == 0.f) {
            m_Power = We;
            m_fPathPdf = 0.f;
            m_nDepth = 0u;
            return;
        }
        We /= raySample.pdf;

        m_Power = We;
        m_nDepth = 1u;

        if(!m_Intersection) {
            m_fPathPdf = raySample.pdf;
            m_fPdfWrtArea = 0.f;
            m_fdVC = 0.f;
            m_fdVCM = 0.f;
        } else {
            m_BSDF.init(-raySample.value.dir, m_Intersection, scene);
            m_fPathPdf = rayOriginPdf * intersectionPdfWrtArea;
            m_fPdfWrtArea = intersectionPdfWrtArea;
            m_fdVCM = mis(pathCount / m_fPdfWrtArea);
            m_fdVC = mis(pathCount * rayOriginToIncidentDirJacobian / m_fPathPdf);
        }
    }

    // Create a primary light vertex
    template<typename MisFunctor>
    BDPTPathVertex(const Scene& scene,
                   const PowerBasedLightSampler& sampler,
                   float lightSample,
                   const Vec2f& positionSample,
                   const Vec2f& directionSample,
                   const Light*& pLight,
                   float& lightPdf,
                   uint32_t pathCount,
                   MisFunctor&& mis) {
        pLight = sampler.sample(scene, lightSample, lightPdf);

        if(!pLight) {
            lightPdf = 0.f;
            m_Power = zero<Vec3f>();
            m_fPathPdf = 0.f;
            m_nDepth = 0u;
            return;
        }

        RaySample raySample;
        float rayOriginPdf, intersectionPdfWrtArea,
                rayOriginToIncidentDirJacobian;
        auto Le = pLight->sampleExitantRay(scene, positionSample,
                                           directionSample, raySample, rayOriginPdf, m_Intersection,
                                           rayOriginToIncidentDirJacobian,
                                           intersectionPdfWrtArea);

        if(Le == zero<Vec3f>() || raySample.pdf == 0.f) {
            m_Power = Le;
            m_fPathPdf = 0.f;
            m_nDepth = 0u;
            return;
        }

        raySample.pdf *= lightPdf;
        Le /= raySample.pdf;

        m_Power = Le;
        m_nDepth = 1u;

        if(!m_Intersection) {
            m_fPathPdf = 0.f;
            m_fPdfWrtArea = 0.f;
            m_fdVC = 0.f;
            m_fdVCM = 0.f;
        } else {
            m_BSDF.init(-raySample.value.dir, m_Intersection, scene);
            m_fPathPdf = rayOriginPdf * intersectionPdfWrtArea;
            m_fPdfWrtArea = intersectionPdfWrtArea;
            m_fdVCM = mis(pathCount / m_fPdfWrtArea);
            m_fdVC = mis(pathCount * rayOriginToIncidentDirJacobian / m_fPathPdf);
        }
    }

    // Extend to the next vertex of the path
    template<typename RandomGenerator, typename MisFunctor>
    bool extend(BDPTPathVertex& next,
                const Scene& scene,
                uint32_t pathCount,
                bool sampleAdjoint, // Number of paths sampled with the strategies s/t = m_nDepth + 1, t/s = *
                RandomGenerator&& rng,
                MisFunctor&& mis) {
        if(!m_Intersection) {
            return false; // Can't sample from infinity
        }

        Sample3f woSample;
        float cosThetaOutDir;
        uint32_t sampledEvent;
        auto fs = m_BSDF.sample(Vec3f(getFloat(rng), getFloat2(rng)), woSample, cosThetaOutDir, &sampledEvent, sampleAdjoint);

        if(woSample.pdf == 0.f || fs == zero<Vec3f>()) {
            return false;
        }

        float reversePdf = m_BSDF.pdf(woSample.value, true);

        auto nextI = scene.intersect(Ray(m_Intersection, woSample.value));

        if(!nextI) {
            next.m_Intersection = nextI;
            next.m_BSDF.init(-woSample.value, scene);
            next.m_fPdfWrtArea = woSample.pdf;
            next.m_fPathPdf = m_fPathPdf * woSample.pdf;
            next.m_Power = m_Power * abs(cosThetaOutDir) * fs / woSample.pdf;
            next.m_nDepth = m_nDepth + 1;

            auto previousPointToIncidentDirectionJacobian = abs(cosThetaOutDir); // No division by squared distance since the point is at infinity
            if((sampledEvent & ScatteringEvent::Specular) && m_BSDF.isDelta()) {
                next.m_fdVC = mis(previousPointToIncidentDirectionJacobian) * m_fdVC;
                next.m_fdVCM = 0.f;
            } else {
                // We set dVC first in case next == *this, so that the dVCM is unchanged until next line
                next.m_fdVC = mis(previousPointToIncidentDirectionJacobian / next.m_fPdfWrtArea) * (m_fdVCM + mis(reversePdf) * m_fdVC);
                next.m_fdVCM = mis(pathCount / next.m_fPdfWrtArea);
            }
        } else {
            auto incidentDirection = -woSample.value;
            auto sqrLength = sqr(nextI.distance);
            if(sqrLength == 0.f) {
                return false;
            }

            auto pdfWrtArea = woSample.pdf * abs(dot(nextI.Ns, incidentDirection)) / sqrLength;

            if(pdfWrtArea == 0.f) {
                return false;
            }

            next.m_Intersection = nextI;
            next.m_BSDF.init(incidentDirection, nextI, scene);
            next.m_fPdfWrtArea = pdfWrtArea;
            next.m_fPathPdf = m_fPathPdf * pdfWrtArea;
            next.m_Power = m_Power * abs(cosThetaOutDir) * fs / woSample.pdf;

            next.m_nDepth = m_nDepth + 1;

            auto previousPointToIncidentDirectionJacobian = abs(cosThetaOutDir) / sqrLength;
            if((sampledEvent & ScatteringEvent::Specular) && m_BSDF.isDelta()) {
                next.m_fdVC = mis(previousPointToIncidentDirectionJacobian) * m_fdVC;
                next.m_fdVCM = 0.f;
            } else {
                // We set dVC first in case next == *this, so that the dVCM is unchanged until next line
                next.m_fdVC = mis(previousPointToIncidentDirectionJacobian / next.m_fPdfWrtArea) * (m_fdVCM + mis(reversePdf) * m_fdVC);
                next.m_fdVCM = mis(pathCount / next.m_fPdfWrtArea);
            }
        }

        return true;
    }
};

template<typename MisFunctor>
inline Vec3f computeEmittedRadiance(
        const BDPTPathVertex& eyeVertex,
        const Scene& scene,
        const PowerBasedLightSampler& lightSampler,
        size_t pathCount, // The number of paths sampled for the strategy s = 0, t = eyeVertex.m_nDepth + 1
        MisFunctor&& mis) {
    if(eyeVertex.m_Intersection.Le == zero<Vec3f>()) {
        return zero<Vec3f>();
    }

    auto contrib = eyeVertex.m_Power * eyeVertex.m_Intersection.Le;

    // Compute MIS weight
    auto rcpWeight = 1.f;

    if(eyeVertex.m_nDepth > 1u) { // Quick fix since we dont connect emission vertex with sensor vertex
        if(!eyeVertex.m_Intersection) {
            // Hit on environment map (if present) should be handled here
            float pointPdfWrtArea, directionPdfWrtSolidAngle;
            lightSampler.computeEnvironmentLightsPdf(scene, -eyeVertex.m_BSDF.getIncidentDirection(), pointPdfWrtArea, directionPdfWrtSolidAngle);

            rcpWeight += mis(pointPdfWrtArea / pathCount) * (eyeVertex.m_fdVCM + mis(directionPdfWrtSolidAngle) * eyeVertex.m_fdVC);
        } else {
            auto meshID = eyeVertex.m_Intersection.meshID;
            auto lightID = scene.getGeometry().getMesh(meshID).m_nLightID;

            const auto& pLight = scene.getLightContainer().getLight(lightID);
            const auto* pAreaLight = static_cast<const AreaLight*>(pLight.get());

            // Evaluate the pdf of sampling the ray (eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection()) on the area light:
            float pointPdfWrtArea, directionPdfWrtSolidAngle;
            pAreaLight->pdf(eyeVertex.m_Intersection, eyeVertex.m_BSDF.getIncidentDirection(), scene,
                            pointPdfWrtArea, directionPdfWrtSolidAngle);

            pointPdfWrtArea *= lightSampler.pdf(lightID); // Scale by the pdf of chosing the area light

            rcpWeight += mis(pointPdfWrtArea / pathCount) * (eyeVertex.m_fdVCM + mis(directionPdfWrtSolidAngle) * eyeVertex.m_fdVC);
        }
    }

    auto weight = 1.f / rcpWeight;

    return weight * contrib;
}

template<typename MisFunctor>
inline Vec3f connectVertices(
        const BDPTPathVertex& eyeVertex,
        const EmissionVertex& lightVertex,
        const Scene& scene,
        size_t pathCount, // The number of paths sampled for the strategy s = 1, t = eyeVertex.m_nDepth + 1
        MisFunctor&& mis) {
    if(!lightVertex.m_pLight || !lightVertex.m_fLightPdf) {
        return zero<Vec3f>();
    }

    float sampledPointPdfWrtArea, sampledPointToIncidentDirectionJacobian, revPdfWrtArea;
    RaySample shadowRay;
    auto Le = lightVertex.m_pLight->sampleDirectIllumination(scene, lightVertex.m_PositionSample, eyeVertex.m_Intersection, shadowRay,
                                                             sampledPointToIncidentDirectionJacobian, sampledPointPdfWrtArea, revPdfWrtArea);

    if(Le == zero<Vec3f>() || shadowRay.pdf == 0.f) {
        return zero<Vec3f>();
    }

    shadowRay.pdf *= lightVertex.m_fLightPdf;
    sampledPointPdfWrtArea *= lightVertex.m_fLightPdf;

    Le /= shadowRay.pdf;

    float cosAtEyeVertex;
    float eyeDirPdf, eyeRevPdf;
    auto fr = eyeVertex.m_BSDF.eval(shadowRay.value.dir, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

    if(fr == zero<Vec3f>() || scene.occluded(shadowRay.value)) {
        return zero<Vec3f>();
    }

    auto contrib = eyeVertex.m_Power * Le * fr * abs(cosAtEyeVertex);

    auto rcpWeight = 1.f;

    {
        // Evaluate the pdf of sampling the light vertex by sampling the brdf of the eye vertex
       auto pdfWrtArea = eyeDirPdf * sampledPointToIncidentDirectionJacobian;
       rcpWeight += mis(pdfWrtArea / (pathCount * sampledPointPdfWrtArea));
    }

    rcpWeight += mis(revPdfWrtArea / pathCount) * (eyeVertex.m_fdVCM + mis(eyeRevPdf) * eyeVertex.m_fdVC);

    auto weight = 1.f / rcpWeight;

    return weight * contrib;
}

template<typename MisFunctor>
inline Vec3f connectVertices(
        const BDPTPathVertex& eyeVertex,
        const BDPTPathVertex& lightVertex,
        const Scene& scene,
        size_t pathCount, // The number of paths sampled for the strategy s = lightVertex.m_nDepth + 1, t = eyeVertex.m_nDepth + 1
        MisFunctor&& mis) {
    Vec3f incidentDirection;
    float dist;
    auto G = geometricFactor(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);
    if(dist == 0.f) {
        return zero<Vec3f>();
    }
    Ray incidentRay(eyeVertex.m_Intersection, lightVertex.m_Intersection, incidentDirection, dist);

    float cosAtLightVertex, cosAtEyeVertex;
    float lightDirPdf, lightRevPdf;
    float eyeDirPdf, eyeRevPdf;

    auto M = lightVertex.m_BSDF.eval(-incidentDirection, cosAtLightVertex, &lightDirPdf, &lightRevPdf) *
            eyeVertex.m_BSDF.eval(incidentDirection, cosAtEyeVertex, &eyeDirPdf, &eyeRevPdf);

    if(G > 0.f && M != zero<Vec3f>() && !scene.occluded(incidentRay)) {
        auto rcpWeight = 1.f;

        {
            auto pdfWrtArea = eyeDirPdf * abs(cosAtLightVertex) / sqr(dist);
            rcpWeight += mis(pdfWrtArea / pathCount) * (lightVertex.m_fdVCM + mis(lightRevPdf) * lightVertex.m_fdVC);
        }

        {
            auto pdfWrtArea = lightDirPdf * abs(cosAtEyeVertex) / sqr(dist);
            rcpWeight += mis(pdfWrtArea / pathCount) * (eyeVertex.m_fdVCM + mis(eyeRevPdf) * eyeVertex.m_fdVC);
        }

        auto weight = 1.f / rcpWeight;

        auto contrib = eyeVertex.m_Power * lightVertex.m_Power * G * M;

        return weight * contrib;
    }

    return zero<Vec3f>();
}

template<typename MisFunctor>
inline Vec3f connectVertices(
        const BDPTPathVertex& lightVertex,
        const SensorVertex& eyeVertex,
        const Scene& scene,
        size_t pathCount, // The number of paths sampled for the strategy s = lightVertex.m_nDepth + 1, t = 1
        MisFunctor&& mis) {
    RaySample shadowRaySample;
    float revPdfWrtArea;
    auto We = eyeVertex.m_pSensor->sampleDirectImportance(scene, eyeVertex.m_PositionSample, lightVertex.m_Intersection,
                                            shadowRaySample, nullptr, nullptr, nullptr,
                                            &revPdfWrtArea, nullptr);

    if(shadowRaySample.pdf == 0.f || We == zero<Vec3f>()) {
        return zero<Vec3f>();
    }

    float cosThetaOutDir;
    float bsdfRevPdfW;
    const auto fr = lightVertex.m_BSDF.eval(shadowRaySample.value.dir, cosThetaOutDir, nullptr, &bsdfRevPdfW);

    if(cosThetaOutDir == 0.f || fr == zero<Vec3f>() || scene.occluded(shadowRaySample.value)) {
        return zero<Vec3f>();
    }

    We /= shadowRaySample.pdf;
    const float wLight = mis(revPdfWrtArea / pathCount) *
            (lightVertex.m_fdVCM + mis(bsdfRevPdfW) * lightVertex.m_fdVC);
    const float misWeight = 1.f / (wLight + 1.f);

    return misWeight * lightVertex.m_Power * fr * abs(cosThetaOutDir) * We;
}

template<typename MisFunctor, typename RandomGenerator>
EmissionVertex sampleLightPath(
        BDPTPathVertex* pLightPath,
        uint32_t maxLightPathDepth,
        const Scene& scene,
        const PowerBasedLightSampler& lightSampler,
        uint32_t misPathCount, // The number of paths used to estimate an integral
        MisFunctor&& mis,
        RandomGenerator&& rng) {
    std::for_each(pLightPath, pLightPath + maxLightPathDepth,
                  [&](BDPTPathVertex& vertex) { vertex.m_fPathPdf = 0.f; });

    if(maxLightPathDepth > 0u) {
        const Light* pLight = nullptr;
        float lightPdf;
        auto positionSample = getFloat2(rng);
        pLightPath[0] = BDPTPathVertex(scene, lightSampler, getFloat(rng), positionSample,
                                       getFloat2(rng), pLight, lightPdf, misPathCount, mis);
        if(pLightPath->m_fPathPdf) {
            while(pLightPath->m_nDepth < maxLightPathDepth &&
                  pLightPath->extend(*(pLightPath + 1), scene, misPathCount, true, rng, mis)) {
                ++pLightPath;
            }
            if(!pLightPath->m_Intersection) {
                pLightPath->m_fPathPdf = 0.f; // If the last vertex is outside the scene, invalidate it
            }
        }
        return { pLight, lightPdf, positionSample };
    }
    return { nullptr, 0.f, Vec2f(0.f) };
}

template<typename MisFunctor, typename RandomGenerator>
SensorVertex sampleEyePath(
        BDPTPathVertex* pEyePath,
        uint32_t maxEyePathDepth,
        const Scene& scene,
        const Sensor& sensor,
        uint32_t misPathCount, // The number of paths used to estimate an integral
        MisFunctor&& mis,
        RandomGenerator&& rng) {
    std::for_each(pEyePath, pEyePath + maxEyePathDepth,
                  [&](BDPTPathVertex& vertex) { vertex.m_fPathPdf = 0.f; });

    if(maxEyePathDepth > 0u) {
        auto positionSample = getFloat2(rng);
        pEyePath[0] = BDPTPathVertex(scene, sensor, getFloat(rng), positionSample,
                                       getFloat2(rng), misPathCount, mis);
        if(pEyePath->m_Intersection && pEyePath->m_fPathPdf) {
            while(pEyePath->m_nDepth < maxEyePathDepth &&
                  pEyePath->extend(*(pEyePath + 1), scene, misPathCount, false, rng, mis)) {
                ++pEyePath;
            }
        }
        return { &sensor, positionSample };
    }
    return { nullptr, 0.f, Vec2f(0.f) };
}

inline std::size_t computeBPTStrategyOffset(std::size_t pathVertexCount, std::size_t lightVertexCount) {
    assert(pathVertexCount >= 2);
    // For a path with pathVertexCount vertices, there is (pathVertexCount + 1) strategies
    // So the offset for paths with pathVertexCount vertices is sum_{i=2}^{pathVertexCount-1}(i + 1) (at least 2 vertices for a path)
    // wich is equal to sum_{i=3}^{pathVertexCount}(i) = -3 + pathVertexCount * (pathVertexCount + 1) / 2
    auto pathOffset = -3 + pathVertexCount * (pathVertexCount + 1) / 2;
    // Then we add the number of light vertices, which corresponds to the strategy s = lightVertexCount, t = pathVertexCount - lightVertexCount
    return pathOffset + lightVertexCount;
}

inline std::size_t computeBPTStrategyCount(std::size_t maxVertexCount) {
    return computeBPTStrategyOffset(maxVertexCount, maxVertexCount) + 1;
}

inline float noMis(float pdf) {
    return pdf ? 1.f : 0.f;
}

inline float balanceHeuristic(float pdf) {
    return pdf;
}

inline float powerHeuristic2(float pdf) {
    return sqr(pdf);
}

}
