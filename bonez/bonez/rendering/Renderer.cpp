#include "Renderer.hpp"

#include <bonez/sys/threads.hpp>
#include <bonez/image/Image.hpp>

#include <bonez/sampling/patterns.hpp>

namespace BnZ {

void Renderer::init(const Scene& scene,
          const Sensor& sensor,
          Framebuffer& framebuffer,
          const tinyxml2::XMLElement* pSettings) {
    m_nIterationCount = 0;
    m_pScene = &scene;
    m_pSensor = &sensor;
    m_pFramebuffer = &framebuffer;
    m_fFramebufferSize = Vec2f(framebuffer.getSize());

    if(pSettings) {
        loadSettings(*pSettings);
    }

    m_Rng.init(m_nThreadCount, getSeed());

    m_pFramebuffer->removeChannels();
    initFramebuffer();
    m_pFramebuffer->clear();

    doInit();
}

void Renderer::render() {
    m_Rng.setSeed(getSeed() + getIterationCount());

    doRender();

    ++m_nIterationCount;
}

void Renderer::setStatisticsOutput(tinyxml2::XMLElement* pStats) {
    m_pStats = pStats;
}

void Renderer::storeStatistics() {
    // Store settings
}

void Renderer::loadSettings(const tinyxml2::XMLElement& xml) {
    serialize(xml, "seed", m_nSeed);
    serialize(xml, "pathDepthMask", m_nPathDepthMask);
    serialize(xml, "iterationCount", m_nIterationCount);
    serialize(xml, "threadCount", m_nThreadCount);
}

void Renderer::storeSettings(tinyxml2::XMLElement& xml) const {
    serialize(xml, "seed", m_nSeed);
    serialize(xml, "pathDepthMask", m_nPathDepthMask);
    serialize(xml, "iterationCount", m_nIterationCount);
    serialize(xml, "threadCount", m_nThreadCount);
}

void Renderer::exposeIO(GUI& gui) {
    gui.addVarRW(BNZ_GUI_VAR(m_nSeed));
    gui.addVarRW(BNZ_GUI_VAR(m_nPathDepthMask));
    gui.addVarRW(BNZ_GUI_VAR(m_nThreadCount));
    gui.addValue(BNZ_GUI_VAR(m_nIterationCount));
}

}
