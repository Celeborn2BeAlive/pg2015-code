#pragma once

namespace BnZ {

inline float FresnelDielectric(
    float aCosInc,
    float mIOR)
{
    if(mIOR < 0)
        return 1.f;

    float etaIncOverEtaTrans;

    if(aCosInc < 0.f)
    {
        // Coming from inside the object: etaInc = mIOR etaTrans = 1.f (the air)
        aCosInc = -aCosInc;
        etaIncOverEtaTrans = mIOR;
    }
    else
    {
        // Coming from outside the object: etaInc = 1.f etaTrans = mIOR
        etaIncOverEtaTrans = 1.f / mIOR;
    }

    const float sinTrans2 = sqr(etaIncOverEtaTrans) * (1.f - sqr(aCosInc));
    const float cosTrans = sqrt(max(0.f, 1.f - sinTrans2));

    const float term1 = etaIncOverEtaTrans * cosTrans;
    const float rParallel =
        (aCosInc - term1) / (aCosInc + term1);

    const float term2 = etaIncOverEtaTrans * aCosInc;
    const float rPerpendicular =
        (term2 - cosTrans) / (term2 + cosTrans);

    return 0.5f * (sqr(rParallel) + sqr(rPerpendicular));
}

inline float FresnelDielectric(
    float cosInc,
    float cosOut,
    float iorInc,
    float iorTrans)
{
    const auto rParallel =
        (iorTrans * cosInc - iorInc * cosOut) / (iorTrans * cosInc + iorInc * cosOut);

    const auto rPerpendicular =
        (iorInc * cosInc - iorTrans * cosOut) / (iorInc * cosInc + iorTrans * cosOut);

    return 0.5f * (sqr(rParallel) + sqr(rPerpendicular));
}

inline float FresnelConductor(float cosInc, float ior, float absorption) {
    const auto tmp1 = sqr(ior) + sqr(absorption);
    const auto tmp2 = tmp1 * sqr(cosInc);

    const auto rParallel2 = (tmp2 - 2 * ior * cosInc + 1) / (tmp2 + 2 * ior * cosInc + 1);
    const auto rPerpendicular2 = (tmp1 - 2 * ior * cosInc + sqr(cosInc)) / (tmp1 + 2 * ior * cosInc + sqr(cosInc));

    return 0.5f * (sqr(rParallel2) + sqr(rPerpendicular2));
}

}
