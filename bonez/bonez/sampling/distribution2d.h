#pragma once

#include "distribution1d.h"

namespace BnZ
{
    inline size_t getDistribution2DBufferSize(size_t width, size_t height) {
        return height + 1 + height * (width + 1);
    }

    template<typename Functor>
    void buildDistribution2D(const Functor& function, float* pBuffer, size_t width, size_t height) {
        auto rowCDFSize = width + 1;
        auto colCDFSize = height + 1;

        auto ptr = pBuffer + colCDFSize;
        for (auto y = 0u; y < height; ++y) {
            buildDistribution1D([&](uint32_t x) {
                return function(x, y);
            }, ptr, width, pBuffer + y);
            ptr += rowCDFSize;
        }

        buildDistribution1D([&](uint32_t y) { return pBuffer[y]; }, pBuffer, height);
    }

    Sample2f sampleContinuousDistribution2D(const float* pBuffer, size_t width, size_t height, const Vec2f& s2D);

    Sample2u sampleDiscreteDistribution2D(const float* pBuffer, size_t width, size_t height, const Vec2f& s2D);

    float pdfContinuousDistribution2D(const float* pBuffer, size_t width, size_t height, const Vec2f& point);

    float pdfDiscreteDistribution2D(const float* pBuffer, size_t width, size_t height, const Vec2u& pixel);
}
