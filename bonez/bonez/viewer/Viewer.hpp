#pragma once

#include "WindowManager.hpp"
#include "ViewController.hpp"
#include "GUI.hpp"

#include <bonez/scene/ConfigManager.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLImageRenderer.hpp>
#include <bonez/parsing/parsing.hpp>
#include <bonez/rendering/RenderModule.hpp>
#include <bonez/sys/files.hpp>

namespace BnZ {

class Viewer {
    void initScene();

    void storeCamera();

    void initConfigManager();

    void exposeConfigIO();

    void exposeLightsIO();

    void exposeRenderIO();

    virtual void drawFrame() = 0;

    virtual void handleGlobalPreprocessParameters(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output) = 0;
protected:
    struct Settings {
        FilePath m_FilePath; // Path to viewer settings file
        tinyxml2::XMLDocument m_Document; // XML document associated to viewer settings file
        tinyxml2::XMLElement* m_pRoot = nullptr; // Root of m_Document
        Vec2u m_WindowSize;
        Vec2u m_FramebufferSize;
        float m_fFramebufferRatio;
        float m_fSpeedFarRatio = 1.f;

        FilePath m_CacheFilePath;
        tinyxml2::XMLDocument m_CacheDocument;
        tinyxml2::XMLElement* m_pCacheRoot = nullptr;

        Settings(const std::string& applicationName, const FilePath& filepath);
    };

    FilePath m_Path;

    // Viewing settings
    Settings m_Settings;

    WindowManager m_WindowManager;
    GUI m_GUI;
    GLShaderManager m_ShaderManager;

    GLImageRenderer m_GLImageRenderer;

    // Scene
    ConfigManager m_ConfigManager;
    static const size_t CONFIG_NAME_MAX_LENGTH = 256;
    char m_sNewConfigName[CONFIG_NAME_MAX_LENGTH];
    uint32_t m_nCurrentConfig = 0u;

    FilePath m_SceneDocumentFilePath;
    tinyxml2::XMLDocument m_SceneDocument;
    Unique<Scene> m_pScene;
    ProjectiveCamera m_Camera;
    Vec2f m_ZNearFar; // Computed from the scene bounds

    ViewController m_ViewController;
    UpdateFlag m_CameraUpdateFlag;

    // For CPU rendering
    RenderModule m_RenderModule;

    bool m_bGUIHasFocus = false;
    uint32_t m_nFrameID = 0;

public:
    // Called before the render loop
    virtual void setUp() = 0;

    // Called after the render loop
    virtual void tearDown() = 0;

    float getZNear() const {
        return m_ZNearFar.x;
    }

    float getZFar() const {
        return m_ZNearFar.y;
    }

    GUI& getGUI() {
        return m_GUI;
    }

    Viewer(const char* title,
           const FilePath& applicationPath,
           const FilePath& settingsFilePath);

    void run();

    const WindowManager& getWindowManager() const {
        return m_WindowManager;
    }

    const GLShaderManager& getShaderManager() const {
        return m_ShaderManager;
    }

    const ProjectiveCamera& getCamera() const {
        return m_Camera;
    }

    const Scene* getScene() const {
        return m_pScene.get();
    }

    Scene* getScene() {
        return m_pScene.get();
    }

    RenderModule& getRenderModule() {
        return m_RenderModule;
    }

    const RenderModule& getRenderModule() const {
        return m_RenderModule;
    }

    GLImageRenderer& getGLImageRenderer() {
        return m_GLImageRenderer;
    }

    bool renderConfig(
            const FilePath& resultsOutputPath,
            const std::string& configName,
            const std::string& sessionName,
            tinyxml2::XMLDocument* pRenderersDocument,
            const tinyxml2::XMLElement* pReferenceElement);
};

}
