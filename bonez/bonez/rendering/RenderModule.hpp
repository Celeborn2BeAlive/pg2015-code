#pragma once

//#include "viewer/Viewer.hpp"
#include "RendererManager.hpp"
#include <bonez/opengl/GLImageRenderer.hpp>
#include <bonez/scene/ConfigManager.hpp>

#include <bonez/scene/sensors/ParaboloidCamera.hpp>

#include <bonez/viewer/GUI.hpp>

#include <bonez/utils/ColorMap.hpp>

namespace BnZ {

class RenderStatistics;

struct RenderModule {
    RendererManager m_RendererManager;
    Framebuffer m_CPUFramebuffer;
    UpdateFlag m_CameraFlag, m_LightFlag;
    Shared<Renderer> m_CurrentRenderer;

    std::vector<Framebuffer> m_FramebufferList; // Saved framebuffers
    uint32_t m_nFramebufferCount = 0;
    int m_nSelectedFramebuffer = -1;

    bool m_bCPURendering = false;
    bool m_bPause = false;
    float m_fGamma = 1.f;
    uint32_t m_nChannelIdx = 0;

    bool m_bRenderFlag = false;

    Vec2u m_SelectedPixel = Vec2u(0u);

    bool m_bWasCleared = false;
    bool m_bDrawDebugData = true;

    bool m_bApplyHeatMap = false;

    Image m_SmallViewImage { 5, 5 };
    const uint32_t m_sSmallViewPixelSize = 32u;
    GLFramebuffer2D<1, false> m_SmallViewFramebuffer;

    void setUp(const FilePath& path, const tinyxml2::XMLElement* pCacheRootElement, const Vec2u& framebufferSize) {
        m_SmallViewFramebuffer.init(m_SmallViewImage.getSize() * m_sSmallViewPixelSize, { GL_RGB32F });

        // Init Framebuffer
        m_CPUFramebuffer = Framebuffer(framebufferSize);

        if(pCacheRootElement) {
            // Load cache data
            auto pModuleSettings = pCacheRootElement->FirstChildElement("RenderModule");
            if(pModuleSettings) {
                getAttribute(*pModuleSettings, "gamma", m_fGamma);
            }
        }

        // Load renderers cache data
        m_RendererManager.setUp(pCacheRootElement);
    }

    void tearDown(tinyxml2::XMLElement* pCacheRootElement) {
        // Store renderers cache data
        m_RendererManager.tearDown(pCacheRootElement);

        if(pCacheRootElement) {
            auto pModuleSettings = pCacheRootElement->FirstChildElement("RenderModule");
            if(!pModuleSettings) {
                pModuleSettings = pCacheRootElement->GetDocument()->NewElement("RenderModule");
                pCacheRootElement->InsertEndChild(pModuleSettings);
            }
            setAttribute(*pModuleSettings, "gamma", m_fGamma);
        }
    }

    void exposeIO(GUI& gui, GLImageRenderer& imageRenderer) {
        auto pFramebuffer = &m_CPUFramebuffer;
        if(m_nSelectedFramebuffer >= 0 && m_nSelectedFramebuffer < int(m_nFramebufferCount)) {
            pFramebuffer = &m_FramebufferList[m_nSelectedFramebuffer];
        }

        if(auto window = gui.addWindow("RenderModule")) {
            gui.addVarRW(BNZ_GUI_VAR(m_bCPURendering));
            gui.addVarRW(BNZ_GUI_VAR(m_bPause));
            gui.addVarRW(BNZ_GUI_VAR(m_fGamma));

            gui.addCombo("Channel", m_nChannelIdx, pFramebuffer->getChannelCount(), [&](uint32_t channelIdx) {
                return pFramebuffer->getChannelName(channelIdx).c_str();
            });

            gui.addValue(BNZ_GUI_VAR(m_SelectedPixel));
            gui.addValue("PixelValue", getPixelValue(m_SelectedPixel));

            gui.addButton("Render", [this]() { m_bRenderFlag = true; });
            gui.addButton("Clear", [this]() { clearCPUFramebuffer(); });
            gui.addVarRW(BNZ_GUI_VAR(m_bDrawDebugData));

            gui.addButton("Store on list", [this]() {
                m_FramebufferList.emplace_back(m_CPUFramebuffer);
                m_nFramebufferCount = m_FramebufferList.size();
            });

            gui.addValue(BNZ_GUI_VAR(m_nFramebufferCount));
            gui.addVarRW(BNZ_GUI_VAR(m_nSelectedFramebuffer));

            gui.addButton("Remove", [this]() {
                if (m_nSelectedFramebuffer < int(m_nFramebufferCount)) {
                    auto it = begin(m_FramebufferList) + m_nSelectedFramebuffer;
                    m_FramebufferList.erase(it);
                    m_nFramebufferCount = m_FramebufferList.size();
                }
            });

            gui.addSeparator();

            gui.addVarRW(BNZ_GUI_VAR(m_bApplyHeatMap));
        }

        if(auto window = gui.addWindow("RenderModule::ZoomedView")) {
            auto smallViewOrigin = Vec2i(m_SelectedPixel) - Vec2i(m_SmallViewImage.getSize()) / 2;

            if(m_nChannelIdx < pFramebuffer->getChannelCount()){
                auto& image = pFramebuffer->getChannel(m_nChannelIdx);
                for(auto y : range(m_SmallViewImage.getHeight())) {
                    for(auto x : range(m_SmallViewImage.getWidth())) {
                        if(image.contains(
                                    smallViewOrigin.x + x, smallViewOrigin.y + y)) {
                            m_SmallViewImage(x, y) = image(smallViewOrigin.x + x, smallViewOrigin.y + y);
                        } else {
                            m_SmallViewImage(x, y) = Vec4f(1.f);
                        }
                    }
                }

                m_SmallViewFramebuffer.bindForDrawing();
                m_SmallViewFramebuffer.setDrawingViewport();

                imageRenderer.drawImage(m_fGamma, m_SmallViewImage, true, false, GL_NEAREST);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0u);
            }
            Vec2f mousePosInImage;
            if(gui.addImage(m_SmallViewFramebuffer.getColorBuffer(0), 1.f, &mousePosInImage)) {
                if(gui.isMouseClicked(MouseButton::LEFT)) {
                    Vec2i pixel(mousePosInImage / float(m_sSmallViewPixelSize));
                    setSelectedPixel(Vec2u(smallViewOrigin + pixel));
                }
            }
        }

        m_RendererManager.exposeIO(gui);
    }

    void clearCPUFramebuffer() {
        m_bWasCleared = true;
    }

    void doCPURendering(UpdateFlag cameraUpdateFlag, const Camera& camera, const Scene& scene, GLImageRenderer& imageRenderer) {
        auto pRenderer = m_RendererManager.getCurrentRenderer();

        if(pRenderer) {
            if(m_bWasCleared || pRenderer != m_CurrentRenderer || cameraUpdateFlag != m_CameraFlag || scene.getLightContainer().hasChanged(m_LightFlag)) {
                m_CurrentRenderer = pRenderer;
                m_CameraFlag = cameraUpdateFlag;

                m_CurrentRenderer->init(scene, camera, m_CPUFramebuffer);

                m_bWasCleared = false;
            }

            if(m_nSelectedFramebuffer < 0 && (!m_bPause || m_bRenderFlag)) {
                m_CurrentRenderer->render();
                m_bRenderFlag = false;
            }
         
            glViewport(0, 0, m_CPUFramebuffer.getWidth(), m_CPUFramebuffer.getHeight());

            if(!m_bApplyHeatMap) {
                if(m_nSelectedFramebuffer < 0) {
                    imageRenderer.drawFramebuffer(m_fGamma, m_CPUFramebuffer, m_nChannelIdx);
                } else if(m_nSelectedFramebuffer < int(m_nFramebufferCount)) {
                    imageRenderer.drawFramebuffer(m_fGamma, m_FramebufferList[m_nSelectedFramebuffer], m_nChannelIdx);
                }
            } else {
                Image copy;
                if(m_nSelectedFramebuffer < 0) {
                    copy = m_CPUFramebuffer.getChannel(m_nChannelIdx);
                } else if(m_nSelectedFramebuffer < int(m_nFramebufferCount)) {
                    copy = m_FramebufferList[m_nSelectedFramebuffer].getChannel(m_nChannelIdx);
                }
                copy.divideByAlpha();
                for(auto& pixel: copy) {
                    pixel = Vec4f(sampleColorMap(HEAT_MAP, pixel.r), 1.f);
                }
                imageRenderer.drawImage(1.f, copy);
            }
        }
    }

    void drawGLData(const Intersection& pickedIntersection, const Vec3f& incidentDirection, GLDebugRenderer& debugRenderer) {
        if(m_bDrawDebugData) {
            auto pRenderer = m_RendererManager.getCurrentRenderer();

            if(pRenderer == m_CurrentRenderer) {
                Renderer::ViewerData viewerData(debugRenderer);
                viewerData.pickedIntersection = pickedIntersection;
                viewerData.incidentDirection = incidentDirection;
                viewerData.selectedPixel = m_SelectedPixel;
                m_CurrentRenderer->drawGLData(viewerData);
            }
        }
    }

    bool renderConfig(
            const FilePath& resultsDirPath,
            const std::string& configName,
            const std::string& sessionName,
            const ConfigManager& configManager,
            tinyxml2::XMLDocument* pRenderersDocument,
            const tinyxml2::XMLElement* pReferenceElement,
            tinyxml2::XMLDocument& sceneDocument,
            Scene& scene,
            float zNear,
            float zFar,
            const std::function<bool()>& callback,
            const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters);

    Vec3f getPixelValue(const Vec2u& pixel) const {
        auto* pFramebuffer = &m_CPUFramebuffer;
        if(m_nSelectedFramebuffer >= 0u && m_nSelectedFramebuffer < m_FramebufferList.size()) {
            pFramebuffer = &m_FramebufferList[m_nSelectedFramebuffer];
        }

        if(m_nChannelIdx < pFramebuffer->getChannelCount()) {
            auto value = pFramebuffer->getChannel(m_nChannelIdx)(pixel.x, pixel.y);
            return Vec3f(value) / value.w;
        }
        return zero<Vec3f>();
    }

    void setSelectedPixel(const Vec2u& pixel) {
        m_SelectedPixel = pixel;
    }

private:
    bool runCompareRenderGroup(
            const FilePath& resultPath,
            tinyxml2::XMLElement& pGroupElement,
            const tinyxml2::XMLElement* pHeritedSettings,
            const Scene& scene,
            const Camera& camera,
            int iterCount,
            float processingTime,
            float minRMSE,
            uint32_t& renderCount,
            const Image& referenceImage,
            const std::function<bool()>& callback,
            const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters);

    bool renderCompare(
            const FilePath& resultPath,
            const tinyxml2::XMLElement& pRendererSettings,
            const Scene& scene,
            const Camera& camera,
            int iterCount,
            float processingTime,
            float minRMSE,
            uint32_t index,
            const Image& referenceImage,
            const std::function<bool()>& callback,
            const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters);

    bool renderReference(
            tinyxml2::XMLDocument& configDocument,
            tinyxml2::XMLDocument& sceneDocument,
            const tinyxml2::XMLElement* pReferenceElement,
            const FilePath& resultPath,
            const Scene& scene, const Camera& camera,
            const std::function<bool()>& callback,
            const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters,
            Shared<Image>& referenceImage);

    void storeResult(const FilePath& resultDir,
                     uint32_t index,
                     const RenderStatistics& stats,
                     float gamma,
                     const Framebuffer& framebuffer);
};

}
