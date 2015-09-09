#pragma once

#include <bonez/scene/Intersection.hpp>
#include <bonez/scene/shading/BSDF.hpp>

namespace BnZ {

struct ImportanceRecord {
    SurfacePoint m_Intersection;
    BSDF m_BSDF;
    float m_fRegionOfInfluenceRadius;

    ImportanceRecord() = default;

    ImportanceRecord(const SurfacePoint& I, const BSDF& bsdf, float regionOfInfluenceRadius):
        m_Intersection(I), m_BSDF(bsdf), m_fRegionOfInfluenceRadius(regionOfInfluenceRadius) {
    }
};

}
