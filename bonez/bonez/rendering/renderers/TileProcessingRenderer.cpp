#include "TileProcessingRenderer.hpp"

#include <numeric>
#include <bonez/sys/DebugLog.hpp>

namespace BnZ {

void TileProcessingRenderer::doInit() {
    m_RenderTimer = TaskTimer({ "BeginFrame", "TileProcessing", "EndFrame"}, 1);

    m_nSpp = m_Spp.x * m_Spp.y;
    m_FramebufferSize = getFramebufferSize();
    m_TileCount = m_FramebufferSize / m_TileSize +
            Vec2u(m_FramebufferSize % m_TileSize != zero<Vec2u>());
    m_nTileCount = m_TileCount.x * m_TileCount.y;

    m_JitteredDistribution = JitteredDistribution2D(m_Spp.x, m_Spp.y);

    preprocess();
}

void TileProcessingRenderer::doRender() {
    {
        auto timer = m_RenderTimer.start(0);
        beginFrame();
    }

    {
        auto timer = m_RenderTimer.start(1);

        auto processedTileCount = 0u;

        processTiles([&](uint32_t threadID, uint32_t tileID, const Vec4u& viewport) {
            processTile(threadID, tileID, viewport);

            if(m_bDisplayProgress) {
                auto l = debugLock();
                ++processedTileCount;
                std::cerr << "Tile " << processedTileCount << " / " << getTileCount() << " (" << (processedTileCount * 100.f / getTileCount()) << " %)" << std::endl;
            }
        });
    }

    {
        auto timer = m_RenderTimer.start(2);
        endFrame();
    }

    logInvalidMeasurements();
}

void TileProcessingRenderer::logInvalidMeasurements() {
    if(!m_bHasDetectedInvalidMeasurement) {
        for(auto i: range(getFramebuffer().getChannel(0).getPixelCount())) {
            auto value = getFramebuffer().getChannel(0)[i];
            if(isInvalidMeasurementEstimate(value)) {
                auto pixelID = i;
                auto pixel = getPixel(pixelID, getFramebufferSize());
                auto x = pixel.x;
                auto y = pixel.y;
                auto tile = pixel / m_TileSize;
                auto tileID = getTileID(tile);
                auto threadID = tileID % getThreadCount();

                BNZ_START_DEBUG_LOG;
                debugLog() << "Invalid measurement detected..." << std::endl;
                debugLog() << "threadID = " << threadID << std::endl;
                debugLog() << "pixelID = " << pixelID << std::endl;
                debugLog() << "tileID = " << tileID << std::endl;
                debugLog() << "iterID = " << getIterationCount() << std::endl;
                debugLog() << "pixel = (" << x << ", " << y << ")" << std::endl;
                debugLog() << "pixelValue = " << getFramebuffer().getChannel(0)[pixelID] << std::endl;

                m_bHasDetectedInvalidMeasurement = true;
                break;
            }
        }
    }
}

void TileProcessingRenderer::reportInvalidContrib(uint32_t threadID, uint32_t tileID,
                        uint32_t pixelID, const std::function<void()>& callback) const {
    if(!m_bHasDetectedInvalidMeasurement) {
        auto l = debugLock();

        if(!m_bHasDetectedInvalidMeasurement) {
            auto pixel = getPixel(pixelID, getFramebufferSize());
            auto x = pixel.x;
            auto y = pixel.y;

            BNZ_START_DEBUG_LOG;
            debugLog() << "Invalid measurement detected..." << std::endl;
            debugLog() << "threadID = " << threadID << std::endl;
            debugLog() << "pixelID = " << pixelID << std::endl;
            debugLog() << "tileID = " << tileID << std::endl;
            debugLog() << "iterID = " << getIterationCount() << std::endl;
            debugLog() << "pixel = (" << x << ", " << y << ")" << std::endl;
            debugLog() << "pixelValue = " << getFramebuffer().getChannel(0)[pixelID] << std::endl;

            callback();

            m_bHasDetectedInvalidMeasurement = true;
        }
    }
}

void TileProcessingRenderer::exposeIO(GUI& gui) {
    Renderer::exposeIO(gui);

    if (ImGui::CollapsingHeader("TileProcessingRenderer"))
    {
        gui.addVarRW(BNZ_GUI_VAR(m_Spp.x));
        gui.addVarRW(BNZ_GUI_VAR(m_Spp.y));
        gui.addVarRW(BNZ_GUI_VAR(m_TileSize.x));
        gui.addVarRW(BNZ_GUI_VAR(m_TileSize.y));

        gui.addVarRW(BNZ_GUI_VAR(m_bDisplayProgress));
    }

    if (ImGui::CollapsingHeader("Render Timings"))
    {
        auto beginFrameTime = m_RenderTimer.getEllapsedTime<Microseconds>(0);
        auto tileProcessingTime = m_RenderTimer.getEllapsedTime<Microseconds>(1);
        auto endFrameTime = m_RenderTimer.getEllapsedTime<Microseconds>(2);
        gui.addValue("BeginFrame", us2ms(beginFrameTime));
        gui.addValue("TileProcessing", us2ms(tileProcessingTime));
        gui.addValue("EndFrame", us2ms(endFrameTime));
        gui.addValue("RenderTime", us2ms(beginFrameTime + tileProcessingTime + endFrameTime));
        gui.addValue("BeginFramePerIteration", us2ms(beginFrameTime) / getIterationCount());
        gui.addValue("TileProcessingPerIteration", us2ms(tileProcessingTime) / getIterationCount());
        gui.addValue("EndFramePerIteration", us2ms(endFrameTime) / getIterationCount());
        gui.addValue("RenderTimePerIteration", us2ms(beginFrameTime + tileProcessingTime + endFrameTime) / getIterationCount());
    }

    doExposeIO(gui);

    m_Spp.x = max(m_Spp.x, 1u);
    m_Spp.y = max(m_Spp.y, 1u);
    m_nSpp = m_Spp.x * m_Spp.y;
    m_TileSize.x = max(m_TileSize.x, 1u);
    m_TileSize.y = max(m_TileSize.y, 1u);
    m_nTileCount = m_TileCount.x * m_TileCount.y;
    m_JitteredDistribution = JitteredDistribution2D(m_Spp.x, m_Spp.y);
}

void TileProcessingRenderer::loadSettings(const tinyxml2::XMLElement& xml) {
    Renderer::loadSettings(xml);

    serialize(xml, "spp", m_Spp);
    serialize(xml, "tileSize", m_TileSize);
    serialize(xml, "displayProgress", m_bDisplayProgress);
    serialize(xml, "hasDetectedInvalidMeasurement", m_bHasDetectedInvalidMeasurement);

    doLoadSettings(xml);
}

void TileProcessingRenderer::storeSettings(tinyxml2::XMLElement& xml) const {
    Renderer::storeSettings(xml);

    serialize(xml, "spp", m_Spp);
    serialize(xml, "tileSize", m_TileSize);
    serialize(xml, "displayProgress", m_bDisplayProgress);
    const bool copy = m_bHasDetectedInvalidMeasurement;
    serialize(xml, "hasDetectedInvalidMeasurement", copy);

    serialize(xml, "displayProgress", m_bDisplayProgress);

    doStoreSettings(xml);
}

void TileProcessingRenderer::storeStatistics() {
    Renderer::storeStatistics();

    if(auto pStats = getStatisticsOutput()) {
        setChildAttribute(*pStats, "IterationCount", getIterationCount());
        setChildAttribute(*pStats, "TotalSpp", getIterationCount() * getSppCount());
        setChildAttribute(*pStats, "RenderTime", m_RenderTimer);

        auto beginFrameTime = m_RenderTimer.getEllapsedTime<Microseconds>(0);
        auto tileProcessingTime = m_RenderTimer.getEllapsedTime<Microseconds>(1);
        auto endFrameTime = m_RenderTimer.getEllapsedTime<Microseconds>(2);
        setChildAttribute(*pStats, "BeginFrameTimePerIteration", us2ms(beginFrameTime) / getIterationCount());
        setChildAttribute(*pStats, "TileProcessingTimePerIteration",  us2ms(tileProcessingTime) / getIterationCount());
        setChildAttribute(*pStats, "EndFrameTimePerIteration", us2ms(endFrameTime) / getIterationCount());
        setChildAttribute(*pStats, "TotalTimePerIteration", us2ms(beginFrameTime + tileProcessingTime + endFrameTime) / getIterationCount());
    }

    doStoreStatistics();
}

}
