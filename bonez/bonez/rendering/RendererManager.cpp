#include "RendererManager.hpp"
#include "renderers/GeometryRenderer.hpp"
#include "renderers/PrimaryRaysRenderer.hpp"
#include "renderers/IGIRenderer.hpp"

#include "renderers/importance_caching/IGIImportanceCachingRenderer.hpp"
#include "renderers/importance_caching/BDPTImportanceCachingRenderer.hpp"

#include "renderers/VCMRenderer.hpp"

#include "renderers/skeleton_connection/IGISkelBasedConnectionRenderer.hpp"
#include "renderers/skeleton_connection/SkelBDPTEG15Renderer.hpp"
#include "renderers/skeleton_connection/BDPTSkelBasedConnectionMultiDistribRenderer.hpp"
#include "renderers/skeleton_connection/BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer.hpp"
#include "renderers/skeleton_connection/SkelVisibilityCorrelationRenderer.hpp"

#include "renderers/RecursiveMISBDPTRenderer.hpp"
#include "renderers/UniformResamplingRecursiveMISBPTRenderer.hpp"
#include "renderers/PathtraceRenderer.hpp"
#include "renderers/skeleton_based_pathtracing/SkelBasedPathtraceRenderer.hpp"

#include "renderers/InstantRadiosityRenderer.hpp"


#include "renderers/SkeletonMappingRenderer.hpp"

#define BNZ_ADD_RENDERER(RendererType) addRenderer<RendererType>(#RendererType)

namespace BnZ {

void RendererManager::addDefaultRenderers() {
    BNZ_ADD_RENDERER(GeometryRenderer);
    BNZ_ADD_RENDERER(PrimaryRaysRenderer);
    BNZ_ADD_RENDERER(IGIRenderer);
    BNZ_ADD_RENDERER(IGIImportanceCachingRenderer);
    BNZ_ADD_RENDERER(PathtraceRenderer);
    BNZ_ADD_RENDERER(RecursiveMISBDPTRenderer);
    BNZ_ADD_RENDERER(UniformResamplingRecursiveMISBPTRenderer);
    BNZ_ADD_RENDERER(VCMRenderer);
    BNZ_ADD_RENDERER(SkelBDPTEG15Renderer);
    BNZ_ADD_RENDERER(BDPTSkelBasedConnectionMultiDistribRenderer);
    BNZ_ADD_RENDERER(BDPTSkelBasedConnectionMultiDistribMultiNodesRenderer);
    BNZ_ADD_RENDERER(IGISkelBasedConnectionRenderer);
    BNZ_ADD_RENDERER(SkelVisibilityCorrelationRenderer);
    BNZ_ADD_RENDERER(BDPTImportanceCachingRenderer);
    BNZ_ADD_RENDERER(SkeletonMappingRenderer);
    BNZ_ADD_RENDERER(InstantRadiosityRenderer);
    BNZ_ADD_RENDERER(SkelBasedPathtraceRenderer);

    m_nCurrentRendererIdx = 0u;
    m_pCurrentRenderer = getRenderer(m_RendererNames[0u]);
}

void RendererManager::addRenderer(const std::string& name, const RendererFactoryFunction& factoryFunction) {
    auto it = m_RendererMap.find(name);
    if(end(m_RendererMap) == it) {

        Shared<Renderer> pRenderer = nullptr;

        m_RendererMap[name] = std::make_pair(pRenderer, factoryFunction);
        m_RendererNames.emplace_back(name);
    } else {
        throw std::runtime_error("RendererManager::addRenderer - Can't add a renderer with the same name");
    }
}

Unique<Renderer> RendererManager::createRenderer(const std::string& name) const {
    auto it = m_RendererMap.find(name);
    if(end(m_RendererMap) == it) {
        throw std::runtime_error("Unable to create the renderer " + name);
    }
    return (*it).second.second();
}

void RendererManager::exposeIO(GUI& gui) {
    if(auto window = gui.addWindow("Renderer")) {
        gui.addCombo("Current Renderer", m_nCurrentRendererIdx, m_RendererNames.size(), [&](uint32_t idx) {
            return m_RendererNames[idx].c_str();
        });
        m_pCurrentRenderer = getRenderer(m_RendererNames[m_nCurrentRendererIdx]);

        if (m_pCurrentRenderer) {
            m_pCurrentRenderer->exposeIO(gui);
        }
    }
}

Shared<Renderer> RendererManager::getRenderer(const std::string& name) {
    auto it = m_RendererMap.find(name);
    if(end(m_RendererMap) == it) {
        return nullptr;
    }

    if(!(*it).second.first) {
        (*it).second.first = (*it).second.second();

        if(m_pRenderersElement) {
            auto pSettings = m_pRenderersElement->
                    FirstChildElement(name.c_str());
            if(pSettings) {
                (*it).second.first->loadSettings(*pSettings);
            }
        }
    }

    return (*it).second.first;
}

void RendererManager::setUp(const tinyxml2::XMLElement* pCacheRoot) {
    addDefaultRenderers();
    if(pCacheRoot) {
        m_pRenderersElement = pCacheRoot->FirstChildElement("Renderers");
        if(m_pRenderersElement) {
            getAttribute(*m_pRenderersElement, "currentRenderer", m_nCurrentRendererIdx);
        }
    }
}

void RendererManager::tearDown(tinyxml2::XMLElement* pCacheRoot) {
    if(pCacheRoot) {
        auto pDocument = pCacheRoot->GetDocument();
        auto pRenderersElement = pCacheRoot->FirstChildElement("Renderers");
        if(!pRenderersElement) {
            pRenderersElement = pDocument->NewElement("Renderers");
            pCacheRoot->InsertEndChild(pRenderersElement);
        }
        pRenderersElement->DeleteChildren();
        for(const auto& pair: m_RendererMap) {
            auto pElement = pDocument->NewElement(pair.first.c_str());
            setAttribute(*pElement, "name", pair.first.c_str());
            pRenderersElement->InsertEndChild(pElement);
            if(pair.second.first) {
                pair.second.first->storeSettings(*pElement);
            }
        }
        setAttribute(*pRenderersElement, "currentRenderer", m_nCurrentRendererIdx);
    }
}

}
