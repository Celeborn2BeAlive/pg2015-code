#include "Viewer.hpp"

#include <bonez/scene/topology/VSkelLoader.hpp>
#include "imgui/imgui.h"

#include <bonez/opengl/GLScreenFramebuffer.hpp>

#include <bonez/sys/DebugLog.hpp>

namespace BnZ {

Viewer::Settings::Settings(const std::string& applicationName, const FilePath& filepath):
    m_FilePath(filepath) {

    if(tinyxml2::XML_NO_ERROR != m_Document.LoadFile(filepath.c_str())) {
        throw std::runtime_error("Unable to load viewer settings file");
    }

    if(nullptr == (m_pRoot = m_Document.RootElement())) {
        throw std::runtime_error("Invalid viewer settings file format (no root element)");
    }

    auto pWindow = m_pRoot->FirstChildElement("Window");
    if(!pWindow) {
        throw std::runtime_error("Invalid viewer settings file format (no Window element)");
    }

    if(!getAttribute(*pWindow, "width", m_WindowSize.x) ||
            !getAttribute(*pWindow, "height", m_WindowSize.y)) {
        throw std::runtime_error("Invalid viewer settings file format (no width/height params)");
    }

    auto pViewController = m_pRoot->FirstChildElement("ViewController");
    if(pViewController) {
        if(!getAttribute(*pViewController, "speedFarRatio", m_fSpeedFarRatio)) {
            throw std::runtime_error("Invalid viewer settings file format (no speedFarRatio params in ViewController)");
        }
    }

    auto pFramebuffer = m_pRoot->FirstChildElement("Framebuffer");
    if(!getAttribute(*pFramebuffer, "width", m_FramebufferSize.x) ||
            !getAttribute(*pFramebuffer, "height", m_FramebufferSize.y)) {
        throw std::runtime_error("Invalid viewer settings file format (no width/height params for the framebuffer)");
    }
    m_fFramebufferRatio = float(m_FramebufferSize.x) / m_FramebufferSize.y;

    auto cacheDirectory = filepath.directory() + "cache";
    createDirectory(cacheDirectory);
    m_CacheFilePath = cacheDirectory + (applicationName + ".cache.bnz.xml");
    if(tinyxml2::XML_NO_ERROR != m_CacheDocument.LoadFile(m_CacheFilePath.c_str())) {
        auto pRoot = m_CacheDocument.NewElement("Cache");
        m_CacheDocument.InsertFirstChild(pRoot);
    }
    m_pCacheRoot = m_CacheDocument.RootElement();
}

void Viewer::initScene() {
    auto pScene = m_Settings.m_pRoot->FirstChildElement("Scene");
    if(!pScene) {
        std::clog << "No Scene  tag found in viewer settings file" << std::endl;
        return;
    }

    m_SceneDocumentFilePath = m_Path + pScene->Attribute("path");

    if(tinyxml2::XML_NO_ERROR != m_SceneDocument.LoadFile(m_SceneDocumentFilePath.c_str())) {
        throw std::runtime_error("Unable to load scene file");
    }
    pScene = m_SceneDocument.RootElement();
    if(!pScene) {
        throw std::runtime_error("No root element in scene's XML file");
    }
    auto path = m_SceneDocumentFilePath.directory();

    m_pScene = makeUnique<Scene>(pScene, path, m_ShaderManager);

    m_ZNearFar.y = 2.f * length(m_pScene->getGeometry().getBBox().size());
    getAttribute(*pScene, "far", m_ZNearFar.y);
    float nearFarRatio = pScene->FloatAttribute("nearFarRatio");
    m_ZNearFar.x = m_ZNearFar.y * nearFarRatio;

    auto pCamera = m_Settings.m_pCacheRoot->FirstChildElement("ProjectiveCamera");
    Vec3f eye(0), point(0, 0, -1), up(0, 1, 0);
    float fovY = radians(45.f);
    if(pCamera) {
        loadPerspectiveCamera(*pCamera, eye, point, up, fovY);
    }

    m_Camera = ProjectiveCamera(lookAt(eye, point, up), fovY,
                                m_Settings.m_FramebufferSize.x,
                                m_Settings.m_FramebufferSize.y,
                                m_ZNearFar.x, m_ZNearFar.y);
}

void Viewer::storeCamera() {
    auto pCamera = m_Settings.m_pCacheRoot->FirstChildElement("ProjectiveCamera");
    if(!pCamera) {
        pCamera = m_Settings.m_CacheDocument.NewElement("ProjectiveCamera");
        m_Settings.m_pCacheRoot->InsertEndChild(pCamera);
    }
    storePerspectiveCamera(*pCamera, m_Camera);
}

void Viewer::initConfigManager() {
    // Init ConfigManager
    auto pConfigs = m_SceneDocument.RootElement()->FirstChildElement("Configs");
    if(!pConfigs || ! pConfigs->Attribute("path")) {
        throw std::runtime_error("No Configs path specified in scene settings");
    }
    auto configPath = m_SceneDocumentFilePath.directory() + pConfigs->Attribute("path");
    m_ConfigManager.setPath(configPath);
}

void Viewer::exposeConfigIO() {
    if(auto configWindow = m_GUI.addWindow("Configs")) {
        m_GUI.addVarRW(BNZ_GUI_VAR(m_sNewConfigName), std::size(m_sNewConfigName));
        m_GUI.addButton("Save Current Config", [&]() {
            m_nCurrentConfig = m_ConfigManager.storeConfig(m_sNewConfigName, m_Camera, m_pScene->getLightContainer());
        });

        m_GUI.addSeparator();

        const auto& configs = m_ConfigManager.getConfigs();

        if(!configs.empty()) {
            if(m_GUI.addCombo("Config", m_nCurrentConfig, configs.size(), [&](uint32_t i) {
                return configs[i].c_str();
            })) {
                m_ConfigManager.loadConfig(m_nCurrentConfig, m_Camera, *m_pScene,
                    m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                    m_ZNearFar.x, m_ZNearFar.y);
            }
        }
    }
}

void Viewer::exposeLightsIO() {
    if(auto lightWindow = m_GUI.addWindow("Lights")) {
        m_pScene->getLightContainer().exposeIO(m_GUI);
    }
}

bool Viewer::renderConfig(
        const FilePath& resultsOutputPath,
        const std::string& configName,
        const std::string& sessionName,
        tinyxml2::XMLDocument* pRenderersDocument,
        const tinyxml2::XMLElement* pReferenceElement) {

    GLScreenFramebuffer m_ScreenFramebuffer;
    m_ScreenFramebuffer.init(m_RenderModule.m_CPUFramebuffer.getSize());

    auto windowSize = m_WindowManager.getSize();
    auto finalViewport =
            Vec4f(0.5 * (windowSize.x - m_RenderModule.m_CPUFramebuffer.getWidth()),
                  0.5 * (windowSize.y - m_RenderModule.m_CPUFramebuffer.getHeight()),
                  m_RenderModule.m_CPUFramebuffer.getSize());

    glDisable(GL_DEPTH_TEST);

    auto callback = [&]() {
        m_WindowManager.handleEvents();

        if(!m_WindowManager.isOpen()) {
            return false;
        }

        m_GUI.startFrame();

        m_ScreenFramebuffer.bindForDrawing();

        glViewport(0, 0, m_RenderModule.m_CPUFramebuffer.getWidth(), m_RenderModule.m_CPUFramebuffer.getHeight());
        glClear(GL_COLOR_BUFFER_BIT);

        m_GLImageRenderer.drawFramebuffer(m_RenderModule.m_fGamma, m_RenderModule.m_CPUFramebuffer,
                                        0u);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        m_ScreenFramebuffer.bindForReading();
        m_ScreenFramebuffer.setReadBuffer(0);

        glBlitFramebuffer(0, 0, m_RenderModule.m_CPUFramebuffer.getWidth(), m_RenderModule.m_CPUFramebuffer.getHeight(),
                          finalViewport.x, finalViewport.y, finalViewport.x + finalViewport.z, finalViewport.y + finalViewport.w,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        glViewport(0, 0, windowSize.x, windowSize.y);
        m_bGUIHasFocus = m_GUI.draw();

        m_WindowManager.swapBuffers();

        return true;
    };

    BNZ_START_DEBUG_LOG;
    debugLog() << "Start rendering resultsOutputPath='" << resultsOutputPath << "' configName='" << configName << "'" << std::endl;

    auto handleGlobalPreprocessParameters = [&](const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output) {
        this->handleGlobalPreprocessParameters(input, output);
    };

    return m_RenderModule.renderConfig(resultsOutputPath,
                                configName,
                                sessionName,
                                m_ConfigManager,
                                pRenderersDocument,
                                pReferenceElement,
                                m_SceneDocument,
                                *m_pScene,
                                m_ZNearFar.x,
                                m_ZNearFar.y,
                                callback,
                                handleGlobalPreprocessParameters);
}

Viewer::Viewer(const char* title, const FilePath& applicationPath, const FilePath& settingsFilePath):
    m_Path(settingsFilePath.directory()),
    m_Settings(applicationPath.file(), settingsFilePath),
    m_WindowManager(m_Settings.m_WindowSize.x, m_Settings.m_WindowSize.y,
                    title),
    m_GUI(applicationPath.file(), m_WindowManager),
    m_ShaderManager(applicationPath.directory() + "shaders"),
    m_GLImageRenderer(m_ShaderManager),
    m_ViewController(m_WindowManager.getWindow()) {

    std::clog << "applicationPath = " << applicationPath << std::endl;
    std::clog << "settingsFilePath = " << settingsFilePath << std::endl;

    initScene();
    initConfigManager();

    m_ViewController.setSpeed(m_Settings.m_fSpeedFarRatio * length(size(m_pScene->getBBox())));

    std::clog << "Number of system threads = " << getSystemThreadCount() << std::endl;

    m_RenderModule.setUp(m_Path,
                         m_Settings.m_pCacheRoot,
                         m_Settings.m_FramebufferSize);

    {
        // Load current config
        auto pConfig = m_Settings.m_pCacheRoot->FirstChildElement("Config");
        if(pConfig) {
            std::string name;
            if(getAttribute(*pConfig, "name", name)) {
                try {
                    ProjectiveCamera camera;
                    m_ConfigManager.loadConfig(name, camera, *m_pScene,
                        m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                        m_ZNearFar.x, m_ZNearFar.y);
                    m_nCurrentConfig = m_ConfigManager.getConfigIndex(name);
                } catch(...) {
                    std::cerr << "Unable to load previously cached configuration" << std::endl;
                }
            }
        } else {
            auto configCount = m_ConfigManager.getConfigs().size();
            if(configCount > 0u) {
                ProjectiveCamera camera;
                m_ConfigManager.loadConfig(0, camera, *m_pScene,
                    m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                    m_ZNearFar.x, m_ZNearFar.y);
                m_nCurrentConfig = 0;
            } else {
                LOG(INFO) << "No configs for the scene, the program might eventually crash.";
            }
        }
    }
}

void Viewer::run() {
    m_GUI.loadSettings(*m_Settings.m_pCacheRoot);

    setUp();

    m_nFrameID = 0;
    while (m_WindowManager.isOpen()) {
        auto startTime = m_WindowManager.getSeconds();

        m_WindowManager.handleEvents();

        m_GUI.startFrame();

        m_Camera.exposeIO(m_GUI);
        exposeConfigIO();
        exposeLightsIO();

        m_ViewController.setViewMatrix(m_Camera.getViewMatrix());

        drawFrame();

        auto size = m_WindowManager.getSize();
        glViewport(0, 0, (int)size.x, (int)size.y);
        m_bGUIHasFocus = m_GUI.draw();

        m_WindowManager.swapBuffers();

        if (!m_bGUIHasFocus) {
            float ellapsedTime = float(m_WindowManager.getSeconds() - startTime);
            if(m_ViewController.update(ellapsedTime)) {
                m_Camera = ProjectiveCamera(m_ViewController.getViewMatrix(), m_Camera.getFovY(),
                                            m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                                            getZNear(), getZFar());
                m_CameraUpdateFlag.addUpdate();
            }
        }

        ++m_nFrameID;
    }

    tearDown();

    m_RenderModule.tearDown(m_Settings.m_pCacheRoot);
    storeCamera();

    if(m_nCurrentConfig < m_ConfigManager.getConfigs().size()) {
        // Store current config
        auto pConfig = m_Settings.m_pCacheRoot->FirstChildElement("Config");
        if(!pConfig) {
            pConfig = m_Settings.m_CacheDocument.NewElement("Config");
            m_Settings.m_pCacheRoot->InsertEndChild(pConfig);
        }
        setAttribute(*pConfig, "name", m_ConfigManager.getConfigName(m_nCurrentConfig));
    }

    m_GUI.storeSettings(*m_Settings.m_pCacheRoot);

    m_Settings.m_CacheDocument.SaveFile(m_Settings.m_CacheFilePath.c_str());
}

}
