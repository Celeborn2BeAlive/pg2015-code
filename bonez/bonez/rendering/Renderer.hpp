#pragma once

#include <bonez/types.hpp>
#include <bonez/parsing/parsing.hpp>
#include <bonez/sys/memory.hpp>
#include <bonez/sys/threads.hpp>
#include <bonez/sampling/Random.hpp>
#include <bonez/viewer/GUI.hpp>

#include "Framebuffer.hpp"

namespace BnZ {

class GLDebugRenderer;

class Renderer {
public:
    virtual ~Renderer() = default;

    /**
     * @brief init Initialize the renderer. Must be call when the scene or the camera is modified.
     * @param scene The scene to render.
     * @param camera The camera expressing the viewpoint.
     * @param framebuffer The framebuffer in which to render the image.
     * @param pSettings Settings for the renderer.
     */
    void init(const Scene& scene,
              const Sensor& sensor,
              Framebuffer& framebuffer,
              const tinyxml2::XMLElement* pSettings = nullptr);

    /**
     * @brief render Render the image. Calling multiple times this function allows for progressive rendering.
     */
    void render();

    /**
     * @brief setStatisticsOutput Set an output for statistics computed by the renderer.
     * @param pStats
     */
    void setStatisticsOutput(tinyxml2::XMLElement* pStats);

    virtual void storeStatistics();

    virtual void loadSettings(const tinyxml2::XMLElement& xml);

    virtual void storeSettings(tinyxml2::XMLElement& xml) const;

    virtual void exposeIO(GUI& gui);

    struct ViewerData {
        GLDebugRenderer& debugRenderer;

        Intersection pickedIntersection;
        Vec3f incidentDirection;
        Vec2u selectedPixel;

        ViewerData(GLDebugRenderer& debugRenderer):
            debugRenderer(debugRenderer) {
        }
    };

    virtual void drawGLData(const ViewerData& viewerData) {
        // Do nothing
    }

    uint32_t getIterationCount() const {
        return m_nIterationCount;
    }

protected:
    struct ThreadRNG {
        const Renderer& m_Renderer;
        uint32_t m_nThreadID;

        ThreadRNG(const Renderer& renderer, uint32_t threadID):
            m_Renderer(renderer), m_nThreadID(threadID) {
        }

        Vec3f getFloat3() const {
            return m_Renderer.getFloat3(m_nThreadID);
        }

        Vec2f getFloat2() const {
            return m_Renderer.getFloat2(m_nThreadID);
        }

        float getFloat() const {
            return m_Renderer.getFloat(m_nThreadID);
        }
    };

    friend Vec3f getFloat3(const ThreadRNG& rng) {
        return rng.getFloat3();
    }

    friend Vec2f getFloat2(const ThreadRNG& rng) {
        return rng.getFloat2();
    }

    friend float getFloat(const ThreadRNG& rng) {
        return rng.getFloat();
    }

    uint32_t getThreadCount() const {
        return m_nThreadCount;
    }

    uint32_t getSeed() const {
        return m_nSeed;
    }

    ThreadsRandomGenerator& getRandomGenerator() const {
        return m_Rng;
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
        return BnZ::getPixelIndex(x, y, getFramebuffer().getSize());
    }

    bool isDebugPixel(uint32_t pixelIdx) const {
        return pixelIdx == getPixelIndex(m_nDebugPixelX, m_nDebugPixelY);
    }

    bool isDebugPixel(uint32_t x, uint32_t y) const {
        return isDebugPixel(getPixelIndex(x, y));
    }

    const Scene& getScene() const {
        assert(m_pScene);
        return *m_pScene;
    }

    const Sensor& getSensor() const {
        assert(m_pSensor);
        return *m_pSensor;
    }

    Vec2u getFramebufferSize() const {
        return m_pFramebuffer->getSize();
    }

    size_t getPixelCount() const {
        return m_pFramebuffer->getPixelCount();
    }

    const Framebuffer& getFramebuffer() const {
        assert(m_pFramebuffer);
        return *m_pFramebuffer;
    }

    void accumulate(uint32_t channelIdx, uint32_t pixelIdx, const Vec4f& value) const {
        assert(m_pFramebuffer);
        m_pFramebuffer->accumulate(channelIdx, pixelIdx, value);
    }

    std::size_t addFramebufferChannel(const std::string& name) const {
        assert(m_pFramebuffer);
        return m_pFramebuffer->addChannel(name);
    }

    std::size_t getFramebufferChannelCount() const {
        assert(m_pFramebuffer);
        return m_pFramebuffer->getChannelCount();
    }

    uint32_t getPathDepthMask() const {
        return m_nPathDepthMask;
    }

    bool acceptPathDepth(uint32_t depth) const {
        return ~m_nPathDepthMask & (1 << depth);
    }

    tinyxml2::XMLElement* getStatisticsOutput() const {
        return m_pStats;
    }

private:
    // Called during init after the framebuffer has been set
    virtual void initFramebuffer() = 0;

    // Called at the end of init
    virtual void doInit() = 0;

    virtual void doRender() = 0;

    // Render context:
    const Scene* m_pScene = nullptr;
    const Sensor* m_pSensor = nullptr;
    Framebuffer* m_pFramebuffer = nullptr;
    tinyxml2::XMLElement* m_pStats = nullptr; // If set, the renderer can output some statistics

    // For multithreading:
    uint32_t m_nSeed = 0u;
    uint32_t m_nThreadCount = BnZ::getSystemThreadCount();
    mutable ThreadsRandomGenerator m_Rng;

    Vec2f m_fFramebufferSize;

    int32_t m_nDebugPixelX = -1;
    int32_t m_nDebugPixelY = -1;

    uint32_t m_nPathDepthMask = 0u;

    uint32_t m_nIterationCount = 0;
};

}
