#pragma once

#include <bonez/scene/Scene.hpp>
#include <bonez/scene/sensors/Sensor.hpp>

#include <bonez/rendering/renderers/recursive_mis_bdpt.hpp>
#include <bonez/rendering/renderers/DirectImportanceSampleTilePartionning.hpp>

namespace BnZ {

using EmissionVertexBuffer = std::vector<EmissionVertex>;
using LightVertexBuffer = Array2d<BDPTPathVertex>;

struct PG15RendererParams {
    const Scene& m_Scene;
    const Sensor& m_Sensor;
    Vec2u m_FramebufferSize;

    std::size_t m_nMaxDepth;
    std::size_t m_nResamplingPathCount;

    Vec2u m_TileSize = Vec2u(32u, 32u);
    Vec2u m_TileCount;
    std::size_t m_nTileCount;

    PG15RendererParams(const Scene& scene, const Sensor& sensor,
                       Vec2u framebufferSize,
                       std::size_t maxDepth, std::size_t resamplingPathCount):
        m_Scene(scene),
        m_Sensor(sensor),
        m_FramebufferSize(framebufferSize),
        m_nMaxDepth(maxDepth),
        m_nResamplingPathCount(resamplingPathCount) {

        m_TileCount = m_FramebufferSize / m_TileSize +
                Vec2u(m_FramebufferSize % m_TileSize != zero<Vec2u>());
        m_nTileCount = m_TileCount.x * m_TileCount.y;
    }
};

struct PG15SharedData {
    const std::size_t m_nLightPathCount;
    const std::size_t m_nLightPathMaxDepth;
    EmissionVertexBuffer m_EmissionVertexBuffer;
    LightVertexBuffer m_LightVertexBuffer;
    DirectImportanceSampleTilePartionning m_DirectImportanceSampleTilePartionning;
    PowerBasedLightSampler m_LightSampler;

    std::size_t m_nIterationCount = 0;

    PG15SharedData(std::size_t lightPathCount, std::size_t lightPathMaxDepth):
        m_nLightPathCount(lightPathCount), m_nLightPathMaxDepth(lightPathMaxDepth),
        m_EmissionVertexBuffer(lightPathCount), m_LightVertexBuffer(lightPathMaxDepth, lightPathCount) {
    }
};

struct ThreadRNG {
    ThreadsRandomGenerator& m_Rng;
    uint32_t m_nThreadID;

    ThreadRNG(ThreadsRandomGenerator& rng, uint32_t threadID):
        m_Rng(rng), m_nThreadID(threadID) {
    }

    Vec3f getFloat3() const {
        return m_Rng.getFloat3(m_nThreadID);
    }

    Vec2f getFloat2() const {
        return m_Rng.getFloat2(m_nThreadID);
    }

    float getFloat() const {
        return m_Rng.getFloat(m_nThreadID);
    }
};

inline Vec3f getFloat3(const ThreadRNG& rng) {
    return rng.getFloat3();
}

inline Vec2f getFloat2(const ThreadRNG& rng) {
    return rng.getFloat2();
}

inline float getFloat(const ThreadRNG& rng) {
    return rng.getFloat();
}

inline float Mis(float pdf) {
    return balanceHeuristic(pdf);
}

class PG15Renderer {
public:
    const Framebuffer& getFramebuffer() const {
        return m_Framebuffer;
    }
protected:
    PG15Renderer(const PG15RendererParams& params,
                 const PG15SharedData& sharedData):
        m_Params(params),
        m_SharedData(sharedData),
        m_Framebuffer(params.m_FramebufferSize) {
        m_Rng.init(getSystemThreadCount(), m_nSeed);
    }

    ~PG15Renderer() = default;

    const PG15RendererParams& m_Params;
    const PG15SharedData& m_SharedData;
    Framebuffer m_Framebuffer;

    std::size_t m_nIterationCount = 0u;

    std::size_t getIterationCount() const {
        return m_nIterationCount;
    }

    template<typename TileProcessingFunc>
    void processTiles(const TileProcessingFunc& fun) {
        auto task = [&](uint32_t threadID) {
            auto loopID = 0u;
            while(true) {
                auto tileID = loopID * getSystemThreadCount() + threadID;
                ++loopID;

                if(tileID >= m_Params.m_nTileCount) {
                    return;
                }

                uint32_t tileX = tileID % m_Params.m_TileCount.x;
                uint32_t tileY = tileID / m_Params.m_TileCount.x;

                Vec2u tileOrg = Vec2u(tileX, tileY) * m_Params.m_TileSize;
                auto viewport = Vec4u(tileOrg, m_Params.m_TileSize);

                if(viewport.x + viewport.z > m_Params.m_FramebufferSize.x) {
                    viewport.z = m_Params.m_FramebufferSize.x - viewport.x;
                }

                if(viewport.y + viewport.w > m_Params.m_FramebufferSize.y) {
                    viewport.w = m_Params.m_FramebufferSize.y - viewport.y;
                }

                fun(threadID, tileID, viewport);
            }
        };

        launchThreads(task, getSystemThreadCount());
    }

    // Process each pixel of a tile with a specific task
    template<typename Task>
    void processTilePixels(const Vec4u& viewport, Task&& task) const {
        auto xEnd = viewport.x + viewport.z;
        auto yEnd = viewport.y + viewport.w;

        for(auto y = viewport.y; y < yEnd; ++y) {
            for(auto x = viewport.x; x < xEnd; ++x) {
                task(x, y);
            }
        }
    }

    ThreadRNG getThreadRNG(std::size_t threadID) const {
        return ThreadRNG(m_Rng, threadID);
    }

    Vec3f getFloat3(uint32_t threadID) const {
        return m_Rng.getFloat3(threadID);
    }

    Vec2f getFloat2(uint32_t threadID) const {
        return m_Rng.getFloat2(threadID);
    }

    float getFloat(uint32_t threadID) const {
        return m_Rng.getFloat(threadID);
    }

    uint32_t getPixelIndex(uint32_t x, uint32_t y) const {
        return BnZ::getPixelIndex(x, y, m_Framebuffer.getSize());
    }

    Framebuffer& getFramebuffer() {
        return m_Framebuffer;
    }

    const Sensor& getSensor() const {
        return m_Params.m_Sensor;
    }

    const Scene& getScene() const {
        return m_Params.m_Scene;
    }

    const Vec2u& getFramebufferSize() const {
        return m_Framebuffer.getSize();
    }

    void accumulate(uint32_t channelIdx, uint32_t pixelIdx, const Vec4f& value) {
        m_Framebuffer.accumulate(channelIdx, pixelIdx, value);
    }

    std::size_t getLightPathCount() const {
        return m_SharedData.m_nLightPathCount;
    }

    std::size_t getMaxEyePathDepth() const {
        return m_Params.m_nMaxDepth;
    }

    std::size_t getMaxLightPathDepth() const {
        return m_SharedData.m_nLightPathMaxDepth;
    }

    std::size_t getMaxDepth() const {
        return m_Params.m_nMaxDepth;
    }

    const BDPTPathVertex* getLightPathPtr(std::size_t index) const {
        return m_SharedData.m_LightVertexBuffer.getSlicePtr(index);
    }

    const BDPTPathVertex* getLightVertexPtr(std::size_t index) const {
        return &m_SharedData.m_LightVertexBuffer[index];
    }

    const Array2d<BDPTPathVertex>& getLightVertexBuffer() const {
        return m_SharedData.m_LightVertexBuffer;
    }

    const PowerBasedLightSampler& getLightSampler() const {
        return m_SharedData.m_LightSampler;
    }

    std::size_t addFramebufferChannel(const std::string& name) {
        return m_Framebuffer.addChannel(name);
    }

    const DirectImportanceSampleTilePartionning::DirectImportanceSampleVector&
        getDirectImportanceSamples(std::size_t tileID) {
        return m_SharedData.m_DirectImportanceSampleTilePartionning[tileID];
    }

    const EmissionVertex* getEmissionVertexBufferPtr() const {
        return m_SharedData.m_EmissionVertexBuffer.data();
    }

    const EmissionVertex& getEmissionVertex(std::size_t index) const {
        return m_SharedData.m_EmissionVertexBuffer[index];
    }

    std::size_t getResamplingLightPathCount() const {
        return m_Params.m_nResamplingPathCount;
    }

    ThreadsRandomGenerator& getRandomGenerator() {
        return m_Rng;
    }

private:
    uint32_t m_nSeed = 42u;
    mutable ThreadsRandomGenerator m_Rng;
};

}
