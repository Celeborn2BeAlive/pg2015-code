#include "BSDF.hpp"

namespace BnZ {

BSDF::BSDF(
    const Vec3f &incidentDirection,
    const SurfacePoint &point,
    const Scene &scene) {
    init(incidentDirection, point, scene);
}

void BSDF::init(const Vec3f &incidentDirection, const Scene &scene) {
    mIncidentDirection = incidentDirection;

    mNormal = zero<Vec3f>();
    mCosThetaIncidentDir = 0.f;
    mSamplingNormal = mNormal;
    m_Kd = zero<Vec3f>();
    m_Ks = zero<Vec3f>();
    m_fShininess = 0.f;
    m_SpecularReflectance = zero<Vec3f>();
    m_SpecularTransmittance = zero<Vec3f>();
    m_fIoR = 0.f;
    m_fSpecularAbsorption = 0.f;
}

void BSDF::init(
        const Vec3f &incidentDirection,
        const SurfacePoint &point,
        const Scene &scene) {
    mNormal = point.Ns;
    mIncidentDirection = incidentDirection;

    mCosThetaIncidentDir = dot(mNormal, mIncidentDirection);

    if(mCosThetaIncidentDir < 0.f) {
        mSamplingNormal = -mNormal;
    } else {
        mSamplingNormal = mNormal;
    }

    const auto& material = scene.getGeometry().getMaterial(scene.getGeometry().getMesh(point.meshID));

    m_Kd = material.getDiffuseReflectance(point.texCoords) * one_over_pi<float>();

    m_Ks = material.getGlossyReflectance(point.texCoords);
    m_fShininess = material.getShininess(point.texCoords);

    m_Ks = m_Ks * (m_fShininess + 2) * one_over_two_pi<float>();

    m_SpecularReflectance = material.getSpecularReflectance(point.texCoords);
    m_SpecularTransmittance = material.getSpecularTransmittance(point.texCoords);
    m_fIoR = material.getIndexOfRefraction(point.texCoords);
    m_fSpecularAbsorption = material.getSpecularAbsorption(point.texCoords);

    computeSamplingProbabilities();

    mIsDelta = (mProbabilities.diffProb == 0) && (mProbabilities.phongProb == 0);
}

Vec3f BSDF::eval(
    const Vec3f &outgoingDirection,
    float       &cosThetaOutDir,
    float       *oDirectPdfW,
    float       *oReversePdfW) const {
    Vec3f result(0);

    if(oDirectPdfW)  *oDirectPdfW = 0;
    if(oReversePdfW) *oReversePdfW = 0;

    cosThetaOutDir = dot(outgoingDirection, mNormal);

    // The two directions have to be in the same hemisphere
    if(cosThetaOutDir * mCosThetaIncidentDir < 0)
        return result;

    result += evalDiffuse(outgoingDirection, oDirectPdfW, oReversePdfW);
    result += evalGlossy(outgoingDirection, oDirectPdfW, oReversePdfW);

    return result;
}

float BSDF::pdf(
    const Vec3f &outgoingDirection,
    const bool  evalReversePdf) const {
    auto cosThetaOutDir = dot(outgoingDirection, mNormal);
    if(cosThetaOutDir * mCosThetaIncidentDir < 0)
        return 0.f;

    float directPdfW  = 0;
    float reversePdfW = 0;

    pdfDiffuse(outgoingDirection, &directPdfW, &reversePdfW);
    pdfGlossy(outgoingDirection, &directPdfW, &reversePdfW);

    return evalReversePdf ? reversePdfW : directPdfW;
}

Vec3f BSDF::sample(
    const Vec3f &aRndTriplet,
    Sample3f& outgoingDir,
    float       &cosThetaOutDir,
    uint32_t        *oSampledEvent,
    bool sampleAdjoint) const {
    uint32_t sampledEvent = ScatteringEvent::Absorption;

    if(aRndTriplet.z < mProbabilities.diffProb)
        sampledEvent = ScatteringEvent::Diffuse | ScatteringEvent::Reflection;
    else if(aRndTriplet.z < mProbabilities.diffProb + mProbabilities.phongProb)
        sampledEvent = ScatteringEvent::Glossy | ScatteringEvent::Reflection;
    else if(aRndTriplet.z < mProbabilities.diffProb + mProbabilities.phongProb + mProbabilities.reflProb)
        sampledEvent = ScatteringEvent::Specular | ScatteringEvent::Reflection;
    else if(aRndTriplet.z < mProbabilities.diffProb + mProbabilities.phongProb + mProbabilities.reflProb + mProbabilities.refrProb)
        sampledEvent = ScatteringEvent::Specular | ScatteringEvent::Transmission;

    if(oSampledEvent)
        *oSampledEvent = sampledEvent;

    switch(sampledEvent) {
    case ScatteringEvent::Diffuse | ScatteringEvent::Reflection:
        return sampleDiffuse(Vec2f(aRndTriplet), cosThetaOutDir, outgoingDir);
    case ScatteringEvent::Glossy | ScatteringEvent::Reflection:
        return sampleGlossy(Vec2f(aRndTriplet), cosThetaOutDir, outgoingDir);
    case ScatteringEvent::Specular | ScatteringEvent::Reflection:
        return sampleReflect(cosThetaOutDir, outgoingDir);
    case ScatteringEvent::Specular | ScatteringEvent::Transmission:
        return sampleRefract(cosThetaOutDir, outgoingDir, sampleAdjoint);
    }

    return zero<Vec3f>();
}

Vec3f BSDF::sampleDiffuse(
    const Vec2f &rndTuple,
    float &cosThetaOutDir,
    Sample3f& outgoingDirection) const
{
    outgoingDirection = cosineSampleHemisphere(rndTuple.x, rndTuple.y, mSamplingNormal);
    cosThetaOutDir = pi<float>() * outgoingDirection.pdf;
    outgoingDirection.pdf *= mProbabilities.diffProb;

    return m_Kd;
}

Vec3f BSDF::sampleGlossy(
    const Vec2f &rndTuple,
    float &cosThetaOutDir,
    Sample3f& outgoingDirection) const
{
    Vec3f R = reflect(mIncidentDirection, mSamplingNormal);

    outgoingDirection = powerCosineSampleHemisphere(rndTuple.x, rndTuple.y, R, m_fShininess);
    cosThetaOutDir = dot(outgoingDirection.value, mSamplingNormal);

    if(cosThetaOutDir <= 0.f) {
        return zero<Vec3f>();
    }

    auto reflectCos = max(0.f, dot(outgoingDirection.value, R));
    return reflectCos ? m_Ks * pow(reflectCos, m_fShininess) : zero<Vec3f>();
}

Vec3f BSDF::sampleReflect(
    float &cosThetaOutDir,
    Sample3f& outgoingDirection) const
{
    outgoingDirection.value = reflect(mIncidentDirection, mNormal);
    outgoingDirection.pdf = mProbabilities.reflProb;

    cosThetaOutDir = dot(mNormal, outgoingDirection.value);

    if(cosThetaOutDir == 0.f) {
        return zero<Vec3f>();
    }

    // BSDF is multiplied (outside) by cosine (oLocalDirGen.z),
    // for mirror this shouldn't be done, so we pre-divide here instead
    return mReflectCoeff * m_SpecularReflectance / abs(cosThetaOutDir);
}

Vec3f BSDF::sampleRefract(
    float &cosThetaOutDir,
    Sample3f& outgoingDirection,
    bool sampleAdjoint) const
{
    if(m_fIoR <= 0)
        return Vec3f(0);

    float etaIncOverEtaTrans;

    if(mCosThetaIncidentDir < 0.f) {
        // hit from inside
        etaIncOverEtaTrans = m_fIoR;
    }
    else {
        etaIncOverEtaTrans = 1.f / m_fIoR;
    }

    outgoingDirection.value = refract(mIncidentDirection, mNormal, etaIncOverEtaTrans);
    outgoingDirection.pdf = mProbabilities.refrProb;

    if(outgoingDirection.value == zero<Vec3f>()) {
        // Total internal reflexion
        return Vec3f(0.f);
    }

    cosThetaOutDir = dot(outgoingDirection.value, mNormal);

    if(cosThetaOutDir == 0.f) {
        return zero<Vec3f>();
    }

    const auto refractCoeff = (1.f - mReflectCoeff) * m_SpecularTransmittance;
    // only camera paths are multiplied by this factor, and etas
    // are swapped because radiance flows in the opposite direction
    if(!sampleAdjoint)
        return Vec3f(refractCoeff * sqr(etaIncOverEtaTrans) / abs(cosThetaOutDir));
    else
        return Vec3f(refractCoeff / abs(cosThetaOutDir));

    return Vec3f(0.f);
}

Vec3f BSDF::evalDiffuse(
    const Vec3f    &outgoingDirection,
    float          *oDirectPdfW,
    float          *oReversePdfW) const
{
    if(mProbabilities.diffProb == 0)
        return Vec3f(0);

    pdfDiffuse(outgoingDirection, oDirectPdfW, oReversePdfW);

    return m_Kd;
}

Vec3f BSDF::evalGlossy(
    const Vec3f    &outgoingDirection,
    float          *oDirectPdfW,
    float          *oReversePdfW) const
{
    if(mProbabilities.phongProb == 0)
        return Vec3f(0);

    Vec3f R = reflect(mIncidentDirection, mSamplingNormal);
    if(oDirectPdfW || oReversePdfW)
    {
        // the sampling is symmetric
        const float pdfW = mProbabilities.phongProb * powerCosineSampleHemispherePDF(outgoingDirection, R, m_fShininess);

        if(oDirectPdfW)
            *oDirectPdfW  += pdfW;

        if(oReversePdfW)
            *oReversePdfW += pdfW;
    }

    auto reflectCos = max(0.f, dot(outgoingDirection, R));
    return reflectCos ? m_Ks * pow(reflectCos, m_fShininess) : zero<Vec3f>();
}

void BSDF::pdfDiffuse(
    const Vec3f    &outgoingDirection,
    float          *oDirectPdfW,
    float          *oReversePdfW) const
{
    if(mProbabilities.diffProb == 0)
        return;

    if(oDirectPdfW)
        *oDirectPdfW  += mProbabilities.diffProb * cosineSampleHemispherePDF(outgoingDirection, mSamplingNormal);

    if(oReversePdfW)
        *oReversePdfW += mProbabilities.diffProb * cosineSampleHemispherePDF(mIncidentDirection, mSamplingNormal);
}

void BSDF::pdfGlossy(
    const Vec3f    &outgoingDirection,
    float          *oDirectPdfW,
    float          *oReversePdfW) const
{
    if(mProbabilities.phongProb == 0)
        return;

    if(oDirectPdfW || oReversePdfW)
    {
        const Vec3f R = reflect(mIncidentDirection, mSamplingNormal);

        // the sampling is symmetric
        const float pdfW = mProbabilities.phongProb * powerCosineSampleHemispherePDF(outgoingDirection, R, m_fShininess);

        if(oDirectPdfW)
            *oDirectPdfW  += pdfW;

        if(oReversePdfW)
            *oReversePdfW += pdfW;
    }
}

void BSDF::computeSamplingProbabilities()
{
    if(refractAlbedo() == 0.f) {
        if(m_fIoR < 0.f) {
            // Perfect mirror
            mReflectCoeff = 1.f;
        } else {
            // Conductor
            mReflectCoeff = FresnelConductor(mCosThetaIncidentDir, m_fIoR, m_fSpecularAbsorption);
        }
    } else {
        // Dielectric
        mReflectCoeff = FresnelDielectric(mCosThetaIncidentDir, m_fIoR);
    }

    const float albedoDiffuse = diffuseAlbedo();
    const float albedoPhong   = glossyAlbedo();
    const float albedoReflect = mReflectCoeff         * reflectAlbedo();
    const float albedoRefract = (1.f - mReflectCoeff) * refractAlbedo();

    const float totalAlbedo = albedoDiffuse + albedoPhong + albedoReflect + albedoRefract;

    if(totalAlbedo < 1e-9f)
    {
        mProbabilities.diffProb  = 0.f;
        mProbabilities.phongProb = 0.f;
        mProbabilities.reflProb  = 0.f;
        mProbabilities.refrProb  = 0.f;
        mContinuationProb = 0.f;
    }
    else
    {
        mProbabilities.diffProb  = albedoDiffuse / totalAlbedo;
        mProbabilities.phongProb = albedoPhong   / totalAlbedo;
        mProbabilities.reflProb  = albedoReflect / totalAlbedo;
        mProbabilities.refrProb  = albedoRefract / totalAlbedo;
        // The continuation probability is max component from reflectance.
        // That way the weight of sample will never rise.
        // Luminance is another very valid option.
        mContinuationProb =
            reduceMax(m_Kd + m_Ks + mReflectCoeff * m_SpecularReflectance) + (1.f - mReflectCoeff);

        mContinuationProb = min(1.f, max(0.f, mContinuationProb));
    }
}

}
