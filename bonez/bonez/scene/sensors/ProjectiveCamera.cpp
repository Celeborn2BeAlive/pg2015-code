#include "ProjectiveCamera.hpp"

#include <bonez/scene/Scene.hpp>

namespace BnZ {

ProjectiveCamera::ProjectiveCamera(const Mat4f& viewMatrix,
                 float fovy, float resX, float resY,
                 float zNear, float zFar):
    Camera(viewMatrix),
    m_ProjMatrix(perspective(fovy, resX / resY, zNear, zFar)),
    m_RcpProjMatrix(inverse(m_ProjMatrix)),
    m_fFovY(fovy),
    m_fResX(resX),
    m_fResY(resY),
    m_fZNear(zNear),
    m_fZFar(zFar) {

    m_fDistanceToImagePlane = m_fZNear;

    auto H = 2 * m_fZNear * tan(0.5f * m_fFovY); // Height of the near plane
    auto W = H * m_fResX / m_fResY; // Width of the near plane
    m_fNDCToImageFactor = 4.f / (W * H); // The jacobian is J = (W / 2) * (H / 2), conversion factor is 1 / J
}

//Ray ProjectiveCamera::doGetRay(const Vec2f& ndcPosition) const {
//    auto pixelWP = getRcpViewProjMatrix() * Vec4f(ndcPosition, -1.f, 1.f); // Sample the near plane z = -1 in NDC space
//    pixelWP /= pixelWP.w;
//    auto org = getPosition();
//    return Ray(org, normalize(Vec3f(pixelWP) - org));
//}

//bool ProjectiveCamera::getNDC(const Vec3f& P, Vec2f& ndc) const {
//    auto P_clip = getViewProjMatrix() * Vec4f(P, 1.f);
//    auto P_ndc = P_clip.xyz() / P_clip.w;
//    if(abs(P_ndc).x > 1.f || abs(P_ndc.y) > 1.f || abs(P_ndc.z) > 1.f) {
//        return false;
//    }
//    ndc = P_ndc.xy();

//    return true;
//}

//float ProjectiveCamera::imageToSurfaceJacobian(const SurfacePoint& point) const {
//    auto directionToCamera = getPosition() - point.P;
//    auto dist = length(directionToCamera);
//    if(dist == 0.f) {
//        return 0.f;
//    }
//    directionToCamera /= dist;
//    const float cosToCamera = abs(dot(directionToCamera, point.Ns));
//    const float cosAtCamera = dot(getFrontVector(), -directionToCamera);
//    if(cosAtCamera <= 0.f) {
//        return 0.f;
//    }

//    const float surfaceToSolidAngleFactor = sqr(dist) / cosToCamera; // Inverse of the jacobian of the transform surface -> camera rays

//    const float imagePointToCameraDist = m_fDistanceToImagePlane / cosAtCamera;
//    const float solidAngleToImagePlaneFactor = cosAtCamera / sqr(imagePointToCameraDist);

//    return surfaceToSolidAngleFactor * solidAngleToImagePlaneFactor;
//}

//float ProjectiveCamera::imageToNdcJacobian(const Vec2f& ndc) const {
//    return m_fNDCToImageFactor;
//}

//float ProjectiveCamera::ndcToRayJacobian(const Vec2f& ndc) const {
//    auto ndcToImageJacobian = 1.f / m_fNDCToImageFactor;

//    auto imagePoint = getRcpViewProjMatrix() * Vec4f(ndc, -1.f, 1.f); // Sample the near plane z = -1 in NDC space
//    imagePoint /= imagePoint.w;

//    auto directionToPoint = Vec3f(imagePoint) - getPosition();
//    auto distanceToPoint = length(directionToPoint);
//    directionToPoint /= distanceToPoint;

//    auto cosAtCamera = dot(getFrontVector(), directionToPoint);

//    return ndcToImageJacobian * cosAtCamera / sqr(distanceToPoint);
//}

// Sample an exitant ray
Vec3f ProjectiveCamera::sampleExitantRay(const Scene& scene,
                                         const Vec2f& lensSample,
                                         const Vec2f& imageSample,
                                         RaySample& raySample,
                                         float& positionPdf,
                                         float& directionPdf,
                                         Intersection& I,
                                         float& sampledPointToIncidentDirectionJacobian,
                                         float& intersectionPdfWrtArea) const {
    auto ndc = uvToNDC(imageSample);

    auto imagePoint = getRcpViewProjMatrix() * Vec4f(ndc, -1.f, 1.f); // Sample the near plane z = -1 in NDC space
    imagePoint /= imagePoint.w;
    auto org = getOrigin();
    auto ray = Ray(org, normalize(Vec3f(imagePoint) - org));

    auto directionToPoint = Vec3f(imagePoint) - getOrigin();
    auto distanceToPoint = length(directionToPoint);
    directionToPoint /= distanceToPoint;

    auto cosAtCamera = dot(getFrontVector(), directionToPoint);

    auto ndcToImageJacobian = 1.f / m_fNDCToImageFactor;
    auto imageToRayJacobian = 4.f * ndcToImageJacobian * cosAtCamera / sqr(distanceToPoint);

    raySample.value = ray;
    raySample.pdf = 1.f / imageToRayJacobian;

    positionPdf = 1.f;
    directionPdf = raySample.pdf;
    sampledPointToIncidentDirectionJacobian = 0.f;

    I = scene.intersect(ray);
    if(I) {
        intersectionPdfWrtArea = directionPdf * abs(dot(I.Ns, -raySample.value.dir)) / sqr(I.distance);
    } else {
        intersectionPdfWrtArea = 0.f;
    }

    // Filter function is the pdf, which integrate to 1 over the space of integration
    return Vec3f(raySample.pdf);
}

// Sample an exitant ray
Vec3f ProjectiveCamera::sampleExitantRay(
            const Scene& scene,
            const Vec2f& lensSample,
            const Vec2f& imageSample,
            RaySample& raySample,
            float& positionPdf,
            float& directionPdf) const {
    auto ndc = uvToNDC(imageSample);

    auto imagePoint = getRcpViewProjMatrix() * Vec4f(ndc, -1.f, 1.f); // Sample the near plane z = -1 in NDC space
    imagePoint /= imagePoint.w;
    auto org = getOrigin();
    auto ray = Ray(org, normalize(Vec3f(imagePoint) - org));

    auto directionToPoint = Vec3f(imagePoint) - getOrigin();
    auto distanceToPoint = length(directionToPoint);
    directionToPoint /= distanceToPoint;

    auto cosAtCamera = dot(getFrontVector(), directionToPoint);

    auto ndcToImageJacobian = 1.f / m_fNDCToImageFactor;
    auto imageToRayJacobian = 4.f * ndcToImageJacobian * cosAtCamera / sqr(distanceToPoint);

    raySample.value = ray;
    raySample.pdf = 1.f / imageToRayJacobian;

    positionPdf = 1.f;
    directionPdf = raySample.pdf;

    // Filter function is the pdf, which integrate to 1 over the space of integration
    return Vec3f(raySample.pdf);
}

Vec3f ProjectiveCamera::sampleDirectImportance(const Scene& scene,
                                               const Vec2f& lensSample,
                                               const SurfacePoint& point,
                                               RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
                                               float* sampledPointToIncidentDirectionJacobian,
                                               float* pdfWrtArea, // Return the pdf of sampling this point, wrt area
                                               float* revPdfWrtSolidAngle, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
                                               float* revPdfWrtArea, // Return the pdf of sampling "point" from the sampled point, wrt area
                                               Vec2f* imageSample) const {
    auto P_clip = getViewProjMatrix() * Vec4f(point.P, 1.f);
    auto P_ndc = Vec3f(P_clip) / P_clip.w;
    if(abs(P_ndc).x > 1.f || abs(P_ndc.y) > 1.f || abs(P_ndc.z) > 1.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }
    auto ndc = Vec2f(P_ndc);

    auto directionToPoint = point.P - getOrigin();
    auto distanceToPoint = length(directionToPoint);
    directionToPoint /= distanceToPoint;

    auto cosAtCamera = dot(getFrontVector(), directionToPoint);

    if(cosAtCamera <= 0.f) {
        shadowRay.pdf = 0.f;
        return zero<Vec3f>();
    }

    shadowRay.value = Ray(point, -directionToPoint, distanceToPoint);
    shadowRay.pdf = sqr(distanceToPoint);

    auto uvToNDCJacobian = 4.f;
    auto ndcToImageJacobian = 1.f / m_fNDCToImageFactor;

    float cosToCamera = abs(dot(-directionToPoint, point.Ns));
    float solidAngleToSurfaceJacobian = cosToCamera == 0.f ? 0.f : sqr(distanceToPoint) / cosToCamera;

    float imagePointToCameraDist = m_fDistanceToImagePlane / cosAtCamera;
    float imageToSolidAngleJacobian = cosAtCamera / sqr(imagePointToCameraDist);

    if(sampledPointToIncidentDirectionJacobian) {
        *sampledPointToIncidentDirectionJacobian = 0.f;
    }

    if(pdfWrtArea) {
        *pdfWrtArea = 1.f;
    }

    if(imageSample) {
        *imageSample = ndcToUV(ndc);
    }

    auto fRevPdfWrtSolidAngle =  1.f / (uvToNDCJacobian * ndcToImageJacobian * imageToSolidAngleJacobian);

    if(revPdfWrtSolidAngle) {
        *revPdfWrtSolidAngle = fRevPdfWrtSolidAngle;
    }

    if(revPdfWrtArea) {
        *revPdfWrtArea = solidAngleToSurfaceJacobian == 0.f ? 0.f : fRevPdfWrtSolidAngle * (1.f / solidAngleToSurfaceJacobian);
    }

    return Vec3f(fRevPdfWrtSolidAngle);
}

}
