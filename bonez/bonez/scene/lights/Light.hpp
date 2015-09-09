#pragma once

#include <bonez/common.hpp>
#include <bonez/types.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/viewer/GUI.hpp>

#include <bonez/scene/Ray.hpp>
#include <bonez/scene/SurfacePoint.hpp>
#include <bonez/scene/Intersection.hpp>

#include "VPLs.hpp"

namespace BnZ {

class Scene;
class DirectLightSampler;
class LightVisitor;

// A Light instance represents a domain of exitant rays with an associated emitted radiance.
//
// Terminology:
// - EmissionVertex: an abstract concept representing a subset of exitant rays:
//      EmissionVertex(emissionVertexSample) = { ray | \exists raySample \in [0,1]^2, ray = sampleExistantRay(emissionVertexSample, raySample) }
//      In the case of finite light sources it generally represents the origin of the subset of rays.
//      In the case of infinite light sources it generally represents the direction of the subset of rays.
class Light {
public:
    virtual ~Light() = default;

    // Compute an upper bound on the power emitted from the light source toward the scene
    virtual Vec3f getPowerUpperBound(const Scene& scene) const = 0;

    virtual void exposeIO(GUI& gui, UpdateFlag& flag) = 0;

    virtual void accept(LightVisitor& visitor) = 0;

    // Sample an exitant ray
    // This function corresponds to a mapping from [0,1]^4 to a four dimensional space of exitant rays
    // \param scene The scene containing the light source
    // \param emissionVertexSample The sampling parameters for sampling the EmissionVertex.
    // \param raySample The sampling parameters for sampling the ray, knowing the EmissionVertex
    // \param raySample Is filled with the exitant ray and its probability density
    // \param emissionVertexPdf Is filled with the probability of sampling the EmissionVertex of the ray. The measure of this density is determined by the
    //      nature of the EmissionVertex (solid angle measure or area measure)
    //
    // \return The emitted radiance along the sampled ray.
    //
    // Properties: The quantity (raySample.pdf / lightVertexPdf) is the probability density of sampling the ray knowing the EmissionVertex.
    virtual Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec2f& raySample,
            RaySample& exitantRaySample,
            float& emissionVertexPdf) const = 0;

    // Sample an exitant ray and trace its intersection with the scene. Compute probabilty densities associated with the intersection.
    // \param intersection Is filled with the intersection of the exitant ray with the scene.
    // \param emissionVertexIncidentDirectionJacobian Is filled with the jacobian of the transformation that map the
    //      EmissionVertex to an incident direction on the intersection.
    // \param intersectionPdfWrtArea Is filled with the probability density of getting the intersection with the sampled exitant ray knowing the EmissionVertex
    //
    // Properties:
    //  - Given a probability density function p(wi) on the incident directions of the intersection, the density of the EmissionVertex is expressed by:
    //          emissionVertexIncidentDirectionJacobian * p(-exitantRaySample.value.dir)
    //  - The pdf of the path (EmissionVertex, intersection) is expressed by emissionVertexPdf * intersectionPdfWrtArea
    virtual Vec3f sampleExitantRay(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec2f& raySample,
            RaySample& exitantRaySample,
            float& emissionVertexPdf,
            Intersection& intersection,
            float& emissionVertexIncidentDirectionJacobian,
            float& intersectionPdfWrtArea) const = 0;

    // Sample an incident ray on a surface point according the direct illumination from this light source.
    // \param scene The scene containing this light source
    // \param emissionVertexSample The sampling parameters for sampling the EmissionVertex that produce the incident direction on "point".
    // \param point The point on which the incident ray must be sampled
    // \param shadowRay Is filled with the sampled ray, its origin is point. The pdf is computed according to the solid angle measure on "point".
    // \param emissionVertexToIncidentDirectionJacobian Is filled with the jacobian of the transformation that map the
    //      EmissionVertex to the sampled incident direction.
    // \param emissionVertexPdf Is filled with the probability density of sampling the EmissionVertex.
    // \param pointPdfWrtArea Is filled with the probability density of sampling "point" knowing the EmissionVertex.
    //
    // \return The emitted radiance along the sampled ray.
    //
    // Properties:
    //  - The following invariant must hold:
    //          Let exitantRay = (org, wo) be an exitant ray sampled with parameters emissionVertexSample and raySample.
    //          Then the incident direction sampled at point = trace(exitantRay) with the parameter emissionVertexSample must be -wo
    //  - Let p(point) be a probability density of sampling point. Then the pdf of the path (point, EmissionVertex) is p(point) * emissionVertexPdf
    virtual Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const SurfacePoint& point,
            RaySample& shadowRay,
            float& emissionVertexToIncidentDirectionJacobian,
            float& emissionVertexPdf,
            float& pointPdfWrtArea
    ) const = 0;

    Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const SurfacePoint& point,
            RaySample& shadowRay) const {
        float emissionVertexToIncidentDirectionJacobian, emissionVertexPdf, pointPdfWrtArea;
        return sampleDirectIllumination(scene, emissionVertexSample, point, shadowRay,
                                        emissionVertexToIncidentDirectionJacobian, emissionVertexPdf, pointPdfWrtArea);
    }

    // Sample an incident ray on an empty space point according the direct illumination from this light source.
    virtual Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec3f& point,
            RaySample& shadowRay,
            float& emissionVertexToIncidentDirectionJacobian,
            float& emissionVertexPdf
    ) const = 0;

    Vec3f sampleDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const Vec3f& point,
            RaySample& shadowRay) const {
        float emissionVertexToIncidentDirectionJacobian, emissionVertexPdf;
        return sampleDirectIllumination(scene, emissionVertexSample, point, shadowRay,
                                        emissionVertexToIncidentDirectionJacobian, emissionVertexPdf);
    }

    virtual Vec3f evalBoundedDirectIllumination(
            const Scene& scene,
            const Vec2f& emissionVertexSample,
            const SurfacePoint& point,
            float radius,
            Ray& shadowRay) const = 0;

    virtual void sampleVPL(const Scene& scene,
                           float lightPdf,
                           const Vec2f& emissionVertexSample,
                           EmissionVPLContainer& container) const = 0;
};

}
