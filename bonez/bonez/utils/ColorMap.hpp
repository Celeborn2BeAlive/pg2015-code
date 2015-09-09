#pragma once

#include <bonez/types.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/sampling/Random.hpp>

namespace BnZ {

typedef std::vector<Vec3f> ColorMap;

/**
 * @brief sample a color computed by linear interpolation
 * @param table The color table
 * @param t The sampling position in [0, 1)
 * @return The sampled color.
 */
inline Vec3f sampleColorMap(const ColorMap& map, float t) {
    t *= (map.size() - 1);
    int i = clamp((int) t, 0, (int) map.size() - 1);
    int j = clamp((int) (i + 1), 0, (int) map.size() - 1);
    float r = t - i;
    return (1 - r) * map[i] + r * map[j];
}

extern const ColorMap HEAT_MAP;

inline ColorMap buildRandomColorMap(size_t count, uint32_t seed = 0) {
    RandomGenerator rng(seed);
    ColorMap map;
    while(count--) {
        map.emplace_back(Vec3f{rng.getFloat(), rng.getFloat(), rng.getFloat()});
    }
    return map;
}

}
