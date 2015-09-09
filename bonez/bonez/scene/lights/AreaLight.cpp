#include "AreaLight.hpp"
#include <bonez/scene/SceneGeometry.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/scene/shading/Material.hpp>
#include <bonez/scene/Scene.hpp>
#include <bonez/sampling/shapes.hpp>
#include "LightVisitor.hpp"

namespace BnZ {

AreaLight::AreaLight(uint32_t meshIdx, const TriangleMesh& mesh, const Material& material):
    m_nMeshIdx(meshIdx) {
    m_Power = material.m_EmittedRadiance * pi<float>() * mesh.getArea();
}

Vec3f AreaLight::getPowerUpperBound(const Scene& scene) const {
    return m_Power;
}

void AreaLight::exposeIO(GUI& gui, UpdateFlag& flag) {
    // Nothing to expose -> materials are not mutable
}

void AreaLight::accept(LightVisitor& visitor) {
    visitor.visit(*this);
}

Vec3f AreaLight::sample(const Scene& scene, float s1D, const Vec2f& s2D,
             SurfacePointSample& surfacePoint) const {
    const auto& mesh = scene.getGeometry().getMesh(m_nMeshIdx);

    auto triangleIdx = clamp(uint32_t(s1D * mesh.getTriangleCount()),
                             0u, uint32_t(mesh.getTriangleCount() - 1));

    const auto& triangle = mesh.getTriangle(triangleIdx);
    auto sampledUV = uniformSampleTriangleUVs(s2D.x, s2D.y,
                                              mesh.getVertex(triangle.v0).position,
                                              mesh.getVertex(triangle.v1).position,
                                              mesh.getVertex(triangle.v2).position);

    scene.getGeometry().getSurfacePoint(m_nMeshIdx, triangleIdx,
                               sampledUV.x, sampledUV.y, surfacePoint.value);
    auto area = mesh.getTriangleArea(triangleIdx);
    surfacePoint.pdf = 1.f / (mesh.getTriangleCount() * area);

    auto Le = scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadiance;
    if(scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadianceTexture) {
        Le *= Vec3f(texture(*scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadianceTexture,
                      surfacePoint.value.texCoords));
    }

    return Le;
}

Vec3f AreaLight::sample(const Scene& scene, Vec2f s2D,
             SurfacePointSample& surfacePoint) const {
    const auto& mesh = scene.getGeometry().getMesh(m_nMeshIdx);

    float s1D = s2D.x;
    auto triangleIdx = clamp(uint32_t(s1D * mesh.getTriangleCount()),
                             0u, uint32_t(mesh.getTriangleCount() - 1));

    s2D.x = s1D * mesh.getTriangleCount() - triangleIdx;

    const auto& triangle = mesh.getTriangle(triangleIdx);
    auto sampledUV = uniformSampleTriangleUVs(s2D.x, s2D.y,
                                              mesh.getVertex(triangle.v0).position,
                                              mesh.getVertex(triangle.v1).position,
                                              mesh.getVertex(triangle.v2).position);

    scene.getGeometry().getSurfacePoint(m_nMeshIdx, triangleIdx,
                               sampledUV.x, sampledUV.y, surfacePoint.value);
    auto area = mesh.getTriangleArea(triangleIdx);
    surfacePoint.pdf = 1.f / (mesh.getTriangleCount() * area);

    auto Le = scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadiance;
    if(scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadianceTexture) {
        Le *= Vec3f(texture(*scene.getGeometry().getMaterial(mesh.m_MaterialID).m_EmittedRadianceTexture,
                      surfacePoint.value.texCoords));
    }

    return Le;
}

float AreaLight::pdf(const SurfacePoint& surfacePoint, const Scene& scene) const {
    const auto& mesh = scene.getGeometry().getMesh(m_nMeshIdx);
    auto area = mesh.getTriangleArea(surfacePoint.triangleID);
    return 1.f / (mesh.getTriangleCount() * area);
}

void AreaLight::pdf(const SurfacePoint& surfacePoint, const Vec3f& outgoingDirection,
            const Scene& scene, float& pointPdf, float& directionPdf) const {
    pointPdf = pdf(surfacePoint, scene);
    directionPdf = cosineSampleHemispherePDF(outgoingDirection, surfacePoint.Ns);
}

Vec3f AreaLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, emissionVertexSample, sampledPoint);
    auto dirSample = cosineSampleHemisphere(raySample.x, raySample.y, sampledPoint.value.Ns);

    emissionVertexPdf = sampledPoint.pdf;

    auto pdf = sampledPoint.pdf * dirSample.pdf;

    exitantRaySample = RaySample(Ray(sampledPoint.value, dirSample.value), pdf);

    return Le * dot(dirSample.value, sampledPoint.value.Ns);
}

Vec3f AreaLight::sampleExitantRay(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec2f& raySample,
        RaySample& exitantRaySample,
        float& emissionVertexPdf,
        Intersection& intersection,
        float& emissionVertexIncidentDirectionJacobian,
        float& intersectionPdfWrtArea) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, emissionVertexSample, sampledPoint);
    auto dirSample = cosineSampleHemisphere(raySample.x, raySample.y, sampledPoint.value.Ns);

    emissionVertexPdf = sampledPoint.pdf;

    auto pdf = sampledPoint.pdf * dirSample.pdf;

    exitantRaySample = RaySample(Ray(sampledPoint.value, dirSample.value), pdf);

    intersection = scene.intersect(exitantRaySample.value);
    if(intersection) {
        emissionVertexIncidentDirectionJacobian = max(0.f, dot(sampledPoint.value.Ns, exitantRaySample.value.dir)) / sqr(intersection.distance);
        intersectionPdfWrtArea = dirSample.pdf * abs(dot(-exitantRaySample.value.dir, intersection.Ns)) / sqr(intersection.distance);
    } else {
        emissionVertexIncidentDirectionJacobian = 0;
        intersectionPdfWrtArea = 0;
    }

    return Le * dot(dirSample.value, sampledPoint.value.Ns);
}

// Sample the direct illumination on point
// Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
// Compute the imageSample associated to lensSample that gived the exitant ray reaching point
Vec3f AreaLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const SurfacePoint& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf,
        float& pointPdfWrtArea
) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, emissionVertexSample, sampledPoint);

    auto wo = point.P - sampledPoint.value.P;
    auto dist = length(wo);
    if(dist == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }
    wo /= dist;
    auto cosOutTheta = max(0.f, dot(wo, sampledPoint.value.Ns));
    if(cosOutTheta == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }

    shadowRay = RaySample(Ray(point, sampledPoint.value, -wo, dist), sampledPoint.pdf * sqr(dist) / cosOutTheta);

    emissionVertexToIncidentDirectionJacobian = max(0.f, dot(sampledPoint.value.Ns, wo)) / sqr(dist);
    emissionVertexPdf = sampledPoint.pdf;
    pointPdfWrtArea = cosineSampleHemispherePDF(wo, sampledPoint.value.Ns) * abs(dot(point.Ns, wo)) /
                sqr(dist);

    return Le;
}

Vec3f AreaLight::sampleDirectIllumination(
        const Scene& scene,
        const Vec2f& emissionVertexSample,
        const Vec3f& point,
        RaySample& shadowRay,
        float& emissionVertexToIncidentDirectionJacobian,
        float& emissionVertexPdf
) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, emissionVertexSample, sampledPoint);

    auto wo = point - sampledPoint.value.P;
    auto dist = length(wo);
    if(dist == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }
    wo /= dist;
    auto cosOutTheta = max(0.f, dot(wo, sampledPoint.value.Ns));
    if(cosOutTheta == 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }

    shadowRay = RaySample(Ray(point, sampledPoint.value, -wo, dist), sampledPoint.pdf * sqr(dist) / cosOutTheta);

    emissionVertexToIncidentDirectionJacobian = max(0.f, dot(sampledPoint.value.Ns, wo)) / sqr(dist);
    emissionVertexPdf = sampledPoint.pdf;

    return Le;
}

Vec3f AreaLight::evalBoundedDirectIllumination(
        const Scene& scene,
        const Vec2f& positionSample,
        const SurfacePoint& point,
        float radius,
        Ray& shadowRay
) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, positionSample, sampledPoint);

    auto wo = point.P - sampledPoint.value.P;
    auto dist = length(wo);
    if(dist == 0.f) {
        return zero<Vec3f>();
    }
    wo /= dist;
    auto cosOutTheta = max(0.f, dot(wo, sampledPoint.value.Ns));
    if(cosOutTheta == 0.f) {
        return zero<Vec3f>();
    }

    shadowRay = Ray(point, sampledPoint.value, -wo, dist);



    auto maxAngleChange = -asin(clamp(radius / dist, 0.f, 1.f));
    auto angleToI = acos(clamp(dot(sampledPoint.value.Ns, wo), -1.f, 1.f));

    return Le * max(0.f, float(cos(angleToI + maxAngleChange))) / (sampledPoint.pdf * sqr(dist));
}

void AreaLight::sampleVPL(
        const Scene& scene,
        float lightPdf,
        const Vec2f& emissionVertexSample,
        EmissionVPLContainer& container) const {
    SurfacePointSample sampledPoint;
    auto Le = sample(scene, emissionVertexSample, sampledPoint);

    container.add(AreaLightVPL {
                      sampledPoint.value.P,
                      sampledPoint.value.Ns,
                      Le / (lightPdf * sampledPoint.pdf)
                  });
}

//AreaLight::VertexSample AreaLight::sampleVertex(
//        const Scene& scene,
//        const Vec2f& positionSample) const {
//    SurfacePointSample sampledPoint;
//    auto Le = sample(scene, positionSample, sampledPoint);

//    AreaLight::VertexSample vertex;
//    vertex.m_PositionOrDirection = sampledPoint.value.P;
//    vertex.m_Normal = sampledPoint.value.Ns;
//    vertex.m_Power = Le;
//    vertex.m_fPdf = sampledPoint.pdf;
//    vertex.m_nMeshID = sampledPoint.value.meshID;
//    vertex.m_nTriangleID = sampledPoint.value.triangleID;

//    return vertex;
//}

//Vec3f AreaLight::sampleDirectIllumination(
//        const Scene& scene,
//        const VertexSample& vertex,
//        const SurfacePoint& point,
//        RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
//        float* sampledPointToIncidentDirectionJacobian,
//        float* pdfWrtArea, // Return the pdf of sampling this point, wrt area
//        float* revPdfWrtSolidAngle, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
//        float* revPdfWrtArea // Return the pdf of sampling "point" from the sampled point, wrt area
//) const {
//    auto wo = point.P - vertex.m_PositionOrDirection;
//    auto dist = length(wo);
//    if(dist == 0.f) {
//        shadowRay.pdf = 0.f;
//        return zero<Vec3f>();
//    }
//    wo /= dist;
//    auto cosOutTheta = max(0.f, dot(wo, vertex.m_Normal));
//    if(cosOutTheta == 0.f) {
//        shadowRay.pdf = 0.f;
//        return zero<Vec3f>();
//    }

//    shadowRay = RaySample(Ray(point, Ray::PrimID{ vertex.m_nMeshID, vertex.m_nTriangleID }, -wo, dist),
//                          vertex.m_fPdf * sqr(dist) / cosOutTheta);

//    if(sampledPointToIncidentDirectionJacobian) {
//        *sampledPointToIncidentDirectionJacobian = cosOutTheta / sqr(dist);
//    }

//    if(pdfWrtArea) {
//        *pdfWrtArea = vertex.m_fPdf;
//    }

//    if(revPdfWrtSolidAngle) {
//        *revPdfWrtSolidAngle = cosineSampleHemispherePDF(wo, vertex.m_Normal);
//    }

//    if(revPdfWrtArea) {
//        *revPdfWrtArea = cosineSampleHemispherePDF(wo, vertex.m_Normal) * abs(dot(point.Ns, wo)) /
//                sqr(dist);
//    }

//    return vertex.m_Power;
//}

}
