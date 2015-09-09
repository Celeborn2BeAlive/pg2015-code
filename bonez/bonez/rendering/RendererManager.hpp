#pragma once

#include "Renderer.hpp"

#include <bonez/common.hpp>
#include <bonez/sys/memory.hpp>

#include <bonez/viewer/GUI.hpp>

namespace BnZ {

// This class manages a list of named renderer and their settings
class RendererManager {
public:
    using RendererFactoryFunction = std::function<Unique<Renderer>()>;
    using RendererPair = std::pair<Shared<Renderer>, RendererFactoryFunction>;

    void addDefaultRenderers();

    void addRenderer(const std::string& name,
                     const RendererFactoryFunction& factoryFunction);

    template<typename RendererType>
    void addRenderer(const std::string& name) {
        addRenderer(name, [] { return makeUnique<RendererType>(); });
    }

    Unique<Renderer> createRenderer(const std::string& name) const;

    void exposeIO(GUI& gui);

    const Shared<Renderer>& getCurrentRenderer() const {
        return m_pCurrentRenderer;
    }

    Shared<Renderer> getRenderer(const std::string& name);

    const std::vector<std::string>& getRendererNames() const {
        return m_RendererNames;
    }

    void setUp(const tinyxml2::XMLElement* pCacheRoot);

    void tearDown(tinyxml2::XMLElement* pCacheRoot);

private:
    const tinyxml2::XMLElement* m_pRenderersElement = nullptr;

    Shared<Renderer> m_pCurrentRenderer;
    uint32_t m_nCurrentRendererIdx = 0;
    std::vector<std::string> m_RendererNames;
    std::unordered_map<std::string, RendererPair> m_RendererMap;
};

}
