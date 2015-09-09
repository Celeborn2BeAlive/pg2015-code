/*
 * Copyright (C) 2012, Tomas Davidovic (http://www.davidovic.cz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * (The above is MIT License: http://en.wikipedia.org/wiki/MIT_License)
 */

#pragma once

#include <vector>
#include <cmath>
#include <bonez/maths/maths.hpp>
#include <bonez/scene/Scene.hpp>
#include <bonez/scene/SurfacePoint.hpp>
#include <bonez/sampling/shapes.hpp>

#include "fresnel.hpp"

//////////////////////////////////////////////////////////////////////////
// BSDF, most magic happens here
//
// One of important conventions is prefixing direction with World when
// are in world coordinates and with Local when they are in local frame,
// i.e., mFrame.
//
// Another important convention if suffix Fix and Gen.
// For PDF computation, we need to know which direction is given (Fix),
// and which is the generated (Gen) direction. This is important even
// when simply evaluating BSDF.
// In BPT, we call Evaluate() when directly connecting to light/camera.
// This gives us both directions required for evaluating BSDF.
// However, for MIS we also need to know probabilities of having sampled
// this path via BSDF sampling, and we need that for both possible directions.
// The Fix/Gen convention (along with Direct and Reverse for PDF) clearly
// establishes which PDF is which.
//
// The BSDF is also templated by direction of tracing, whether from camera
// (BSDF<false>) or from light (BSDF<true>). This is identical to Veach's
// Adjoint BSDF (except the name is more straightforward).
// For us this is only used when refracting.

namespace BnZ {

struct ScatteringEvent {
    enum {
        Emission = 0,
        Absorption = 1,
        Diffuse = 2,
        Glossy = 4,
        Specular = 8,
        Reflection = 16,
        Transmission = 32,
        Other = 64
    };
};

class BSDF
{
    struct ComponentProbabilities
    {
        float diffProb;
        float phongProb;
        float reflProb;
        float refrProb;
    };

public:
    BSDF() = default;

    BSDF(
        const Vec3f &incidentDirection,
        const SurfacePoint &point,
        const Scene &scene);

    void init(const Vec3f &incidentDirection, const Scene &scene);

    void init(
            const Vec3f &incidentDirection,
            const SurfacePoint &point,
            const Scene &scene);

    /* \brief Given a direction, evaluates BSDF
     *
     * Returns value of BSDF, as well as cosine for the
     * aWorldDirGen direction.
     * Can return probability (w.r.t. solid angle W),
     * of having sampled aWorldDirGen given mLocalDirFix (oDirectPdfW),
     * and of having sampled mLocalDirFix given aWorldDirGen (oReversePdfW).
     *
     */
    Vec3f eval(
        const Vec3f &outgoingDirection,
        float       &cosThetaOutDir,
        float       *oDirectPdfW = nullptr,
        float       *oReversePdfW = nullptr) const;

    Vec3f eval(const Vec3f &outgoingDirection) const {
        float cosThetaOutDir;
        return eval(outgoingDirection, cosThetaOutDir);
    }

    /* \brief Given a direction, evaluates Pdf
     *
     * By default returns PDF with which would be aWorldDirGen
     * generated from mLocalDirFix. When aEvalRevPdf == true,
     * it provides PDF for the reverse direction.
     */
    float pdf(
        const Vec3f &outgoingDirection,
        const bool  evalReversePdf = false) const;

    /* \brief Given 3 random numbers, samples new direction from BSDF.
     *
     * Uses z component of random triplet to pick BSDF component from
     * which it will sample direction. If non-specular component is chosen,
     * it will also evaluate the other (non-specular) BSDF components.
     * Return BSDF factor for given direction, as well as PDF choosing that direction.
     * Can return event which has been sampled.
     * If result is Vec3f(0,0,0), then the sample should be discarded.
     */
    Vec3f sample(
        const Vec3f &aRndTriplet,
        Sample3f& outgoingDir,
        float       &cosThetaOutDir,
        uint32_t        *oSampledEvent = nullptr,
        bool sampleAdjoint = false) const;

    bool isDelta() const {
        return mIsDelta;
    }

    bool hasSpecularComponent() const {
        return mProbabilities.reflProb > 0.f || mProbabilities.refrProb > 0.f;
    }

    bool hasAllComponents(uint32_t scatteringComponents) const {
        bool result = true;
        if(scatteringComponents & ScatteringEvent::Diffuse) {
            result = result || mProbabilities.diffProb > 0.f;
        }
        if(scatteringComponents & ScatteringEvent::Glossy) {
            result = result || mProbabilities.phongProb > 0.f;
        }
        if(scatteringComponents & ScatteringEvent::Specular) {
            result = result || hasSpecularComponent();
        }
        return result;
    }

    Vec3f getDiffuseCoefficient() const {
        return m_Kd;
    }

    Vec3f getGlossyCoefficient() const {
        return m_Ks;
    }

    float getShininess() const {
        return m_fShininess;
    }

    float getContinuationProb() const  {
        return mContinuationProb;
    }

    float getCosThetaIncidentDirection() const  {
        return mCosThetaIncidentDir;
    }

    Vec3f getIncidentDirection() const {
        return mIncidentDirection;
    }

private:

    ////////////////////////////////////////////////////////////////////////////
    // Sampling methods
    // All sampling methods take material, 2 random numbers [0-1[,
    // and return BSDF factor, generated direction in local coordinates, and PDF
    ////////////////////////////////////////////////////////////////////////////

    Vec3f sampleDiffuse(
        const Vec2f &aRndTuple,
        float &cosThetaOutDir,
        Sample3f& outgoingDirection) const;

    Vec3f sampleGlossy(
        const Vec2f &aRndTuple,
        float &cosThetaOutDir,
        Sample3f& outgoingDirection) const;

    Vec3f sampleReflect(
        float &cosThetaOutDir,
        Sample3f& outgoingDirection) const;

    Vec3f sampleRefract(
        float &cosThetaOutDir,
        Sample3f& outgoingDirection,
        bool sampleAdjoint) const;

    ////////////////////////////////////////////////////////////////////////////
    // Evaluation methods
    ////////////////////////////////////////////////////////////////////////////

    Vec3f evalDiffuse(
        const Vec3f    &outgoingDirection,
        float          *oDirectPdfW = NULL,
        float          *oReversePdfW = NULL) const;

    Vec3f evalGlossy(
        const Vec3f    &outgoingDirection,
        float          *oDirectPdfW = NULL,
        float          *oReversePdfW = NULL) const;

    ////////////////////////////////////////////////////////////////////////////
    // Pdf methods
    ////////////////////////////////////////////////////////////////////////////

    void pdfDiffuse(
        const Vec3f    &outgoingDirection,
        float          *oDirectPdfW = NULL,
        float          *oReversePdfW = NULL) const;

    void pdfGlossy(
        const Vec3f    &outgoingDirection,
        float          *oDirectPdfW = NULL,
        float          *oReversePdfW = NULL) const;

    ////////////////////////////////////////////////////////////////////////////
    // Albedo methods
    ////////////////////////////////////////////////////////////////////////////

    float diffuseAlbedo() const
    {
        return luminance(m_Kd);
    }

    float glossyAlbedo() const
    {
        return luminance(m_Ks);
    }

    float reflectAlbedo() const
    {
        return luminance(m_SpecularReflectance);
    }

    float refractAlbedo() const
    {
        return m_fIoR > 0.f ? luminance(m_SpecularTransmittance) : 0.f;
    }

    void computeSamplingProbabilities();

    Vec3f mNormal;            //!< Local frame of reference
    Vec3f mSamplingNormal;
    Vec3f mIncidentDirection;      //!< Incoming (fixed) direction, in local
    float mCosThetaIncidentDir;
    bool  mIsDelta;          //!< True when material is purely specular
    ComponentProbabilities mProbabilities; //!< Sampling probabilities
    float mContinuationProb; //!< Russian roulette probability
    float mReflectCoeff;     //!< Fresnel reflection coefficient (for glass)

    Vec3f m_Kd;
    Vec3f m_Ks;
    float m_fShininess;
    Vec3f m_SpecularReflectance = Vec3f(0.f);
    Vec3f m_SpecularTransmittance = Vec3f(0.f);
    float m_fIoR = -1.f; // Index of refraction
    float m_fSpecularAbsorption = 0.f;
};

}
