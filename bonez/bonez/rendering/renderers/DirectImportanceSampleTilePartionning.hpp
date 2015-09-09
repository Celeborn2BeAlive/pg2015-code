#pragma once

#include <vector>
#include <bonez/types.hpp>
#include <bonez/scene/sensors/Sensor.hpp>

namespace BnZ {

class DirectImportanceSampleTilePartionning {
public:
    struct DirectImportanceSample {
        std::size_t m_nLightVertexIndex; // The index of the light vertex splatted on the sensor
        Vec2f m_LensSample;
        Vec2u m_Pixel;
    };

    using DirectImportanceSampleVector = std::vector<DirectImportanceSample>;

    template<typename GetIntersectionFunctor, typename IsValidFunctor, typename RandomGenerator>
    void build(
            std::size_t count,
            const Scene& scene,
            const Sensor& sensor,
            const Vec2u& framebufferSize,
            const Vec2u& tileSize,
            const Vec2u& tileCount,
            GetIntersectionFunctor&& getIntersection,
            IsValidFunctor&& isValid,
            RandomGenerator&& rng)
    {
        m_TileDirectImportanceSamples.resize(tileCount.x * tileCount.y);
        for(auto& v: m_TileDirectImportanceSamples) {
            v.clear();
        }

        for(auto i = decltype(count)(); i < count; ++i) {
            if(isValid(i)) {
                auto lensSample = rng.getFloat2();
                RaySample shadowRaySample;
                Vec2f imageSample;
                auto We = sensor.sampleDirectImportance(scene, lensSample, getIntersection(i),
                                                        shadowRaySample, nullptr, nullptr, nullptr, nullptr, &imageSample);

                if(We != zero<Vec3f>() && shadowRaySample.pdf > 0.f) {
                    auto pixel = getPixel(imageSample, framebufferSize);

                    auto tile = pixel / tileSize;
                    auto tileID = tile.x + tile.y * tileCount.x;

                    m_TileDirectImportanceSamples[tileID].emplace_back(DirectImportanceSample{ i, lensSample, pixel });
                }
            }
        }
    }

    const DirectImportanceSampleVector& operator [](std::size_t tileID) const {
        return m_TileDirectImportanceSamples[tileID];
    }

private:
    std::vector<DirectImportanceSampleVector> m_TileDirectImportanceSamples;
};

}
