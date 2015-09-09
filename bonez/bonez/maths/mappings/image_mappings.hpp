#pragma once

#include "../common.hpp"
#include "image_mappings.hpp"

namespace BnZ {

inline uint32_t getPixelIndex(uint32_t x, uint32_t y, const Vec2u& imageSize) {
    return x + y * imageSize.x;
}

inline uint32_t getPixelIndex(const Vec2u& pixel, const Vec2u& imageSize) {
    return getPixelIndex(pixel.x, pixel.y, imageSize);
}

inline Vec2u getPixel(uint32_t index, const Vec2u& imageSize) {
    return Vec2u(index % imageSize.x, index / imageSize.x);
}

inline Vec2u getPixel(const Vec2f& uv, const Vec2u& imageSize) {
    return clamp(Vec2u(uv * Vec2f(imageSize)),
                 Vec2u(0),
                 Vec2u(imageSize.x - 1, imageSize.y - 1));
}

inline Vec2u rasterPositionToPixel(const Vec2f& rasterPosition,
                                   const Vec2u& imageSize) {
    return clamp(Vec2u(rasterPosition),
                 Vec2u(0),
                 Vec2u(imageSize.x - 1, imageSize.y - 1));
}

inline Vec2f getUV(const Vec2f& rasterPosition,
                   const Vec2f& imageSize) {
    return rasterPosition / imageSize;
}

inline Vec2f getUV(const Vec2f& rasterPosition,
                   const Vec2u& imageSize) {
    return getUV(rasterPosition, Vec2f(imageSize));
}

inline Vec2f getUV(uint32_t i, uint32_t j,
                   const Vec2u& imageSize) {
    return getUV(Vec2f(i, j) + Vec2f(0.5f),
                 imageSize);
}

inline Vec2f getUV(const Vec2u& pixel,
                   const Vec2u& imageSize) {
    return getUV(Vec2f(pixel) + Vec2f(0.5f),
                 imageSize);
}

inline Vec2f ndcToUV(const Vec2f& ndc) {
    return 0.5f * (ndc + Vec2f(1));
}

inline Vec2f uvToNDC(const Vec2f& uv) {
    return 2.f * uv - Vec2f(1);
}

inline Vec2f getNDC(const Vec2f rasterPosition, Vec2f imageSize) {
    return Vec2f(-1) + 2.f * rasterPosition / imageSize;
}

}
