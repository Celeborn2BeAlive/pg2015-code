#pragma once

#include <bonez/scene/Scene.hpp>
#include <bonez/scene/shading/BSDF.hpp>
#include <bonez/scene/lights/PowerBasedLightSampler.hpp>
#include <bonez/scene/lights/Light.hpp>
#include <bonez/scene/sensors/Sensor.hpp>

namespace BnZ {

class PathVertex {
public:
    PathVertex() = default;

    PathVertex(const Intersection& intersection,
           const BSDF& bsdf,
           uint32_t sampledEvent,
           const Vec3f& power,
           uint32_t length,
           bool sampleAdjoint):
        m_Intersection(intersection),
        m_BSDF(bsdf),
        m_nSampledEvent(sampledEvent),
        m_Power(power),
        m_nLength(length),
        m_bSampleAdjoint(sampleAdjoint) {
    }

    void invalidate() {
        m_nLength = 0u;
    }

    template<typename RandomGenerator>
    bool extend(const Scene& scene, const RandomGenerator& rng) {
        if(!m_Intersection) {
            // Cannot extend from infinity
            invalidate();
            return false;
        }

        Sample3f woSample;
        float cosThetaOutDir;
        uint32_t sampledEvent;
        auto fs = m_BSDF.sample(Vec3f(getFloat(rng), getFloat2(rng)), woSample, cosThetaOutDir, &sampledEvent, m_bSampleAdjoint);

        if(woSample.pdf == 0.f || fs == zero<Vec3f>()) {
            invalidate();
            return false;
        }

        m_nSampledEvent = sampledEvent;
        m_Power *= abs(cosThetaOutDir) * fs / woSample.pdf;
        ++m_nLength;

        auto nextI = scene.intersect(Ray(m_Intersection, woSample.value));

        if(!nextI) {
            m_Intersection = nextI;
            return true;
        }

        auto incidentDirection = -woSample.value;
        auto sqrLength = sqr(nextI.distance);
        if(sqrLength == 0.f) {
            invalidate();
            return false;
        }
        auto pdfWrtArea = woSample.pdf  * abs(dot(nextI.Ns, incidentDirection)) / sqrLength;

        if(pdfWrtArea == 0.f) {
            invalidate();
            return false;
        }

        m_Intersection = nextI;
        m_BSDF.init(incidentDirection, nextI, scene);

        return true;
    }

    const Intersection& intersection() const {
        return m_Intersection;
    }

    const BSDF& bsdf() const {
        return m_BSDF;
    }

    uint32_t sampledEvent() const {
        return m_nSampledEvent;
    }

    const Vec3f& power() const {
        return m_Power;
    }

    std::size_t length() const {
        return m_nLength;
    }
protected:
    Intersection m_Intersection;
    BSDF m_BSDF;
    uint32_t m_nSampledEvent;
    Vec3f m_Power;
    std::size_t m_nLength = 0u;
    bool m_bSampleAdjoint;
};

class BidirPathVertex {
public:
    BidirPathVertex() = default;

    BidirPathVertex(
            const Scene& scene,
            const Intersection& intersection,
            const Vec3f& incidentDirection,
            uint32_t sampledEvent,
            float previousPointToIncidentDirectionJacobian,
            float pdfWrtArea,
            float pathPdf,
            const Vec3f& power,
            uint32_t length,
            bool sampleAdjoint):
        m_Intersection(intersection),
        m_BSDF(incidentDirection, intersection, scene),
        m_nSampledEvent(sampledEvent),
        m_fPreviousPointToIncidentDirectionJacobian(previousPointToIncidentDirectionJacobian),
        m_fPdfWrtArea(pdfWrtArea),
        m_fPathPdf(pathPdf),
        m_Power(power),
        m_nLength(length),
        m_bSampleAdjoint(sampleAdjoint) {
    }

    BidirPathVertex(
            const Intersection& intersection,
            const BSDF& bsdf,
            uint32_t sampledEvent,
            float previousPointToIncidentDirectionJacobian,
            float pdfWrtSolidAngle,
            float pdfWrtArea,
            float pathPdf,
            const Vec3f& power,
            uint32_t length,
            bool sampleAdjoint):
        m_Intersection(intersection),
        m_BSDF(bsdf),
        m_nSampledEvent(sampledEvent),
        m_fPreviousPointToIncidentDirectionJacobian(previousPointToIncidentDirectionJacobian),
        m_fPdfWrtArea(pdfWrtArea),
        m_fPathPdf(pathPdf),
        m_Power(power),
        m_nLength(length),
        m_bSampleAdjoint(sampleAdjoint) {
    }

    void invalidate() {
        m_nLength = 0u;
    }

    template<typename RandomGenerator>
    bool extend(const Scene& scene, const RandomGenerator& rng) {
        Sample3f woSample;
        float cosThetaOutDir;
        uint32_t sampledEvent;
        auto fs = m_BSDF.sample(Vec3f(getFloat(rng), getFloat2(rng)), woSample, cosThetaOutDir, &sampledEvent, m_bSampleAdjoint);

        if(woSample.pdf == 0.f || fs == zero<Vec3f>()) {
            invalidate();
            return false;
        }

        auto nextI = scene.intersect(Ray(m_Intersection, woSample.value));

        if(!nextI) {
            invalidate();
            return false;
        }

        auto incidentDirection = -woSample.value;
        auto sqrLength = sqr(nextI.distance);
        if(sqrLength == 0.f) {
            invalidate();
            return false;
        }
        auto pdfWrtArea = woSample.pdf  * abs(dot(nextI.Ns, incidentDirection)) / sqrLength;

        if(pdfWrtArea == 0.f) {
            invalidate();
            return false;
        }

        m_Intersection = nextI;
        m_BSDF.init(incidentDirection, nextI, scene);
        m_nSampledEvent = sampledEvent;
        m_fPreviousPointToIncidentDirectionJacobian = abs(cosThetaOutDir) / sqrLength;
        m_fPdfWrtArea = pdfWrtArea;

        m_fPathPdf *= pdfWrtArea;
        m_Power *= abs(cosThetaOutDir) * fs / woSample.pdf;

        ++m_nLength;

        return true;
    }

    const Intersection& intersection() const {
        return m_Intersection;
    }

    const BSDF& bsdf() const {
        return m_BSDF;
    }

    uint32_t sampledEvent() const {
        return m_nSampledEvent;
    }

    float previousPointToIncidentDirectionJacobian() const {
        return m_fPreviousPointToIncidentDirectionJacobian;
    }

    float pdfWrtArea() const {
        return m_fPdfWrtArea;
    }

    float pathPdf() const {
        return m_fPathPdf;
    }

    const Vec3f& power() const {
        return m_Power;
    }

    Vec3f& power() {
        return m_Power;
    }

    std::size_t length() const {
        return m_nLength;
    }

protected:
    Intersection m_Intersection;
    BSDF m_BSDF;
    uint32_t m_nSampledEvent;
    float m_fPreviousPointToIncidentDirectionJacobian;
    float m_fPdfWrtArea;
    float m_fPathPdf;
    Vec3f m_Power;
    std::size_t m_nLength = 0u;
    bool m_bSampleAdjoint;
};

template<typename VertexType, typename RandomGenerator>
class Path {
public:
    using Vertex = VertexType;

    struct iterator: public std::iterator<std::input_iterator_tag, Vertex> {
        iterator() = default;

        iterator& operator++() {
            m_Vertex.extend(*m_pScene, *m_pRng);

            return *this;
        }

        const Vertex& operator*() const {
            return m_Vertex;
        }

        iterator operator++(int) {
            iterator copy { *this } ;
            ++(*this);
            return copy;
        }

        const Vertex* operator ->() {
            return &m_Vertex;
        }

        bool operator==(const iterator& rhs) {
            return m_Vertex.length() == rhs.m_Vertex.length();
        }

        bool operator!=(const iterator& rhs) {
            return !(*this == rhs);
        }

        Vertex m_Vertex;
        const RandomGenerator* m_pRng = nullptr;
        const Scene* m_pScene = nullptr;
    };

    Path() = default; // empty path

    Path(const Scene& scene, const Vertex& initialVertex, const RandomGenerator& rng) {
        m_Begin.m_pRng = &rng;
        m_Begin.m_pScene = &scene;
        m_Begin.m_Vertex = initialVertex;
    }

    iterator begin() const {
        return m_Begin;
    }

    iterator end() const {
        return iterator{};
    }

private:
    iterator m_Begin;
};

template<typename VertexType, typename RandomGenerator>
Path<VertexType, RandomGenerator> makePath(const Scene& scene, VertexType sourceVertex, const RandomGenerator& rng) {
    return { scene, sourceVertex, rng };
}

template<typename VertexType = BidirPathVertex>
inline VertexType samplePrimaryLightVertex(const Scene& scene,
                                         const PowerBasedLightSampler& sampler,
                                         float lightSample,
                                         const Vec2f& positionSample,
                                         const Vec2f& directionSample,
                                         const Light*& pLight,
                                         float& lightPdf,
                                         float& positionPdfGivenLight) {
    pLight = sampler.sample(scene, lightSample, lightPdf);

    if(!pLight) {
        lightPdf = 0.f;
        return {};
    }

    RaySample raySample;
    Intersection I;
    float sampledPositionToIncidentDirectionJacobian;
    float intersectionPdfWrtArea;
    auto Le = pLight->sampleExitantRay(scene, positionSample,
                                       directionSample, raySample, positionPdfGivenLight, I, sampledPositionToIncidentDirectionJacobian,
                                       intersectionPdfWrtArea);

    if(!I) {
        return {};
    }

    if(Le == zero<Vec3f>() || raySample.pdf == 0.f) {
        return {};
    }

    raySample.pdf *= lightPdf;
    Le /= raySample.pdf;

    auto pdfIntersection = intersectionPdfWrtArea;
    auto g = sampledPositionToIncidentDirectionJacobian;

    return {
        scene,
        I,
        -raySample.value.dir,
        ScatteringEvent::Emission,
        g,
        pdfIntersection,
        lightPdf * positionPdfGivenLight * pdfIntersection,
        Le,
        1u,
        true };
}

template<typename RandomGenerator, typename VertexType = BidirPathVertex>
Path<VertexType, RandomGenerator> makeLightPath(
        const Scene& scene,
        const PowerBasedLightSampler& sampler,
        const RandomGenerator& rng) {
    const Light* pLight = nullptr;
    float lightPdf, positionPdfGivenLight;

    auto lightSample = rng.getFloat();
    auto positionSample = rng.getFloat2();
    auto dirSample = rng.getFloat2();

    return {
        scene,
        samplePrimaryLightVertex(scene, sampler, lightSample, positionSample,
                                 dirSample, pLight, lightPdf, positionPdfGivenLight),
        rng };
}

template<typename PathVertex>
struct PrimaryEyeVertexHelper;

template<>
struct PrimaryEyeVertexHelper<BidirPathVertex> {
    static BidirPathVertex sample(const Scene& scene,
                           const Sensor& sensor,
                           const Vec2f& lensSample,
                           const Vec2f& imageSample) {
        RaySample raySample;
        Intersection I;
        float positionPdf, directionPdf, intersectionPdfWrtArea,
                sampledPointToIncidentDirectionJacobian;
        auto We = sensor.sampleExitantRay(scene, lensSample, imageSample, raySample, positionPdf, directionPdf, I,
                                          sampledPointToIncidentDirectionJacobian, intersectionPdfWrtArea);

        if(We == zero<Vec3f>() || raySample.pdf == 0.f) {
            return {};
        }
        We /= raySample.pdf;

        if(!I) {
            return {
                I,
                BSDF(),
                ScatteringEvent::Emission,
                0.f,
                directionPdf,
                0.f,
                raySample.pdf,
                We * I.Le,
                0u,
                false
            };
        }

        return {
            I,
            BSDF(-raySample.value.dir, I, scene),
            ScatteringEvent::Emission,
            sampledPointToIncidentDirectionJacobian,
            directionPdf,
            intersectionPdfWrtArea,
            positionPdf * intersectionPdfWrtArea,
            We,
            1u,
            false
        };
    }
};

template<>
struct PrimaryEyeVertexHelper<PathVertex> {
    static PathVertex sample(const Scene& scene,
                           const Sensor& sensor,
                           const Vec2f& lensSample,
                           const Vec2f& imageSample) {
        RaySample raySample;
        Intersection I;
        float positionPdf, directionPdf, intersectionPdfWrtArea,
                sampledPointToIncidentDirectionJacobian;
        auto We = sensor.sampleExitantRay(scene, lensSample, imageSample, raySample, positionPdf, directionPdf, I,
                                          sampledPointToIncidentDirectionJacobian, intersectionPdfWrtArea);

        if(We == zero<Vec3f>() || raySample.pdf == 0.f) {
            return {};
        }
        We /= raySample.pdf;

        if(!I) {
            return {
                I,
                BSDF(),
                ScatteringEvent::Emission,
                We * I.Le,
                0u,
                false
            };
        }

        return {
            I,
            BSDF(-raySample.value.dir, I, scene),
            ScatteringEvent::Emission,
            We,
            1u,
            false
        };
    }
};

template<typename PathVertex>
inline PathVertex samplePrimaryEyeVertex(
                const Scene& scene,
                const Sensor& sensor,
                const Vec2f& lensSample,
                const Vec2f& imageSample) {
    return PrimaryEyeVertexHelper<PathVertex>::sample(scene, sensor, lensSample, imageSample);
}

}
