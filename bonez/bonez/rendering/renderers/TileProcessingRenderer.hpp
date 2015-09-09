#pragma once

#include <bonez/rendering/Renderer.hpp>
#include <bonez/sampling/patterns.hpp>

namespace BnZ {

class TileProcessingRenderer: public Renderer {
public:
    virtual ~TileProcessingRenderer() = default;

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

    void exposeIO(GUI& gui) override;

    void loadSettings(const tinyxml2::XMLElement& xml) override;

    void storeSettings(tinyxml2::XMLElement& xml) const override;

    void storeStatistics() override final;
protected:
    const Vec2u getTileSize() const {
        return m_TileSize;
    }

    Vec2u getSpp() const {
        return m_Spp;
    }

    uint32_t getSppCount() const {
        return m_nSpp;
    }

    uint32_t getTileCount() const {
        return m_nTileCount;
    }

    Vec2u getTileCount2() const {
        return m_TileCount;
    }

    void logInvalidMeasurements();

    void reportInvalidContrib(uint32_t threadID, uint32_t tileID,
                            const uint32_t pixelID,
                            const std::function<void()>& callback) const;

    // Process each sample of each pixel of a tile with a specific task
    template<typename Task>
    void processTileSamples(const Vec4u& viewport, Task&& task) const {
        auto xEnd = viewport.x + viewport.z;
        auto yEnd = viewport.y + viewport.w;

        for(auto y = viewport.y; y < yEnd; ++y) {
            for(auto x = viewport.x; x < xEnd; ++x) {
                uint32_t pixelID = getPixelIndex(x, y);
                for(auto sampleID = 0u; sampleID < m_nSpp; ++sampleID) {
                    task(x, y, pixelID, sampleID);
                }
            }
        }
    }

    Vec2f getPixelSample(uint32_t threadID, uint32_t sampleID) const {
        return m_JitteredDistribution.getSample(sampleID, getFloat2(threadID));
    }

    uint32_t getTileID(const Vec2u& tile) const {
        return tile.x + tile.y * m_TileCount.x;
    }

    template<typename TileProcessingFunc>
    void processTiles(const TileProcessingFunc& fun) {
        auto task = [&](uint32_t threadID) {
            auto loopID = 0u;
            while(true) {
                auto tileID = loopID * getThreadCount() + threadID;
                ++loopID;

                if(tileID >= m_nTileCount) {
                    return;
                }

                uint32_t tileX = tileID % m_TileCount.x;
                uint32_t tileY = tileID / m_TileCount.x;

                Vec2u tileOrg = Vec2u(tileX, tileY) * m_TileSize;
                auto viewport = Vec4u(tileOrg, m_TileSize);

                if(viewport.x + viewport.z > m_FramebufferSize.x) {
                    viewport.z = m_FramebufferSize.x - viewport.x;
                }

                if(viewport.y + viewport.w > m_FramebufferSize.y) {
                    viewport.w = m_FramebufferSize.y - viewport.y;
                }

                fun(threadID, tileID, viewport);
            }
        };

        launchThreads(task, getThreadCount());
    }

private:
    // Do once during initialization
    // Scene, Camera and Framebuffer are not supposed to change until next call to this method
    virtual void preprocess() {
        // Default implementation does nothing
    }

    // Called before the processing of a frame
    virtual void beginFrame() {
        // Default implementation does nothing
    }

    // Process a tile
    virtual void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const = 0;

    // Called after the processing of a frame
    virtual void endFrame() {
        // Default implementation does nothing
    }

    virtual void doExposeIO(GUI& gui) {
    }

    virtual void doLoadSettings(const tinyxml2::XMLElement& xml) {
    }

    virtual void doStoreSettings(tinyxml2::XMLElement& xml) const {
    }

    void doInit() override;

    void doRender() override;

    virtual void doStoreStatistics() const {
        // Do nothing by default
    }

    Vec2u m_TileSize = Vec2u(32u, 32u);
    Vec2u m_TileCount;

    // Timings
    TaskTimer m_RenderTimer;

//    Microseconds m_fInitFrameTime { 0 };
//    Microseconds m_fTileProcessingTime { 0 };
//    Microseconds m_fEndFrameTime { 0 };
//    std::vector<Microseconds> m_PerThreadTileProcessingTime;

    Vec2u m_Spp = Vec2u(1);
    uint32_t m_nSpp = 0;

    Vec2u m_FramebufferSize;

    uint32_t m_nTileCount = 0u;

    bool m_bDisplayProgress = false;

    JitteredDistribution2D m_JitteredDistribution;

    mutable bool m_bHasDetectedInvalidMeasurement = false;
};

}
