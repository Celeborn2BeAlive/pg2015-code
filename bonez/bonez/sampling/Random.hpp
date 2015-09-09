#pragma once

#include <random>
#include <algorithm>
#include <bonez/types.hpp>
#include <bonez/utils/itertools/itertools.hpp>

namespace BnZ {

// Transform a float into a random generator; useful
// to sample standard library distribution with arbitrary floating points
struct RandomFloat {
    typedef float result_type;

    float value = 0.f;

    RandomFloat() = default;

    RandomFloat(float v): value(v) {
    }

    operator float() const {
        return value;
    }

    result_type min() {
        return 0.f;
    }

    result_type max() {
        return 1.f;
    }

    result_type operator()() {
        return value;
    }
};

class RandomGenerator {
    typedef std::mt19937 Generator;
public:
    typedef float result_type;

    RandomGenerator(uint32_t seed = 0u):
        m_Generator(seed), m_Distribution(0, 1), m_nSeed(seed) {
    }

    void setSeed(uint32_t seed) {
        m_Generator.seed(seed);
        m_nSeed = seed;
        m_nCallCount = 0u;
    }

    uint32_t getSeed() const {
        return m_nSeed;
    }

    uint32_t getUInt() {
        return m_Generator();
    }

    float getFloat() {
        ++m_nCallCount;
        // doesn't work on windows: returns number greater than 1...
        //return std::generate_canonical<float, std::numeric_limits<float>::digits>(m_Generator);
        return m_Distribution(m_Generator);
    }

    Vec2f getFloat2() {
        return Vec2f(getFloat(), getFloat());
    }

    Vec3f getFloat3() {
        return Vec3f(getFloat(), getFloat(), getFloat());
    }

    result_type min() {
        return 0.f;
    }

    result_type max() {
        return 1.f;
    }

    result_type operator()() {
        return getFloat();
    }

    uint64_t getCallCount() const {
        return m_nCallCount;
    }

    void discard(uint64_t callCount) {
        m_Generator.discard(callCount);
        m_nCallCount += callCount;
    }

private:
    Generator m_Generator;
    std::uniform_real_distribution<float> m_Distribution;
    uint32_t m_nSeed;
    uint64_t m_nCallCount = 0u;
};

template<typename Distrib>
typename Distrib::result_type sampleDistribution(RandomFloat r, Distrib& d) {
    return d(RandomFloat{r});
}

inline uint32_t getTileSeed(uint32_t frameID, const Vec4u& tileViewport,
                            const Vec2u& imageSize) {
    return tileViewport.x + tileViewport.y * imageSize.x +
            frameID * imageSize.x * imageSize.y;
}

class ThreadsRandomGenerator {
public:
    ThreadsRandomGenerator() = default;

    ThreadsRandomGenerator(uint32_t threadCount, uint32_t seed) {
        init(threadCount, seed);
    }

    void init(uint32_t threadCount, uint32_t seed) {
        m_RandomGenerators.clear();
        m_RandomGenerators.reserve(threadCount);
        for(auto i = 0u; i < threadCount; ++i) {
            m_RandomGenerators.emplace_back(seed * threadCount + i);
        }
    }

    void setSeed(uint32_t seed) {
        const auto threadCount = m_RandomGenerators.size();
        for(auto i: range(threadCount)) {
            m_RandomGenerators[i].setSeed(seed * threadCount + i);
        }
    }

    float getFloat(uint32_t threadID) {
        return m_RandomGenerators[threadID].getFloat();
    }

    Vec2f getFloat2(uint32_t threadID) {
        return m_RandomGenerators[threadID].getFloat2();
    }

    Vec3f getFloat3(uint32_t threadID) {
        return m_RandomGenerators[threadID].getFloat3();
    }

    uint64_t getCallCount(uint32_t threadID) {
        return m_RandomGenerators[threadID].getCallCount();
    }

    void discard(uint32_t threadID, uint64_t callCount) {
        m_RandomGenerators[threadID].discard(callCount);
    }

    uint32_t getSeed(uint32_t threadID) const {
        return m_RandomGenerators[threadID].getSeed();
    }

    void setSeed(uint32_t threadID, uint32_t seed) {
        m_RandomGenerators[threadID].setSeed(seed);;
    }

    RandomGenerator& getGenerator(uint32_t threadID) {
        return m_RandomGenerators[threadID];
    }

private:
    std::vector<RandomGenerator> m_RandomGenerators;
};

}
