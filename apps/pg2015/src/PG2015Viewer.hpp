#pragma once

#include <bonez/viewer/WindowManager.hpp>
#include <bonez/viewer/GUI.hpp>
#include <bonez/scene/ConfigManager.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLImageRenderer.hpp>
#include <bonez/opengl/GLScreenFramebuffer.hpp>
#include <bonez/parsing/parsing.hpp>
#include <bonez/rendering/RenderModule.hpp>
#include <bonez/sys/files.hpp>
#include <bonez/sys/time.hpp>

#include "PG15RendererManager.hpp"

namespace BnZ {

class PG2015Viewer {
public:
    PG2015Viewer(const FilePath& applicationPath,
                 const FilePath& viewerFilePath,
                 const std::string& configName,
                 const FilePath& referenceImagePath,
                 const FilePath& resultPath,
                 std::size_t renderTimeMsOrIterationCount,
                 std::size_t maxPathDepth,
                 std::size_t resamplingPathCount,
                 const PG15ICBPTSettings& icBPTSettings,
                 const std::vector<PG15SkelBPTSettings>& skelBPTSettings,
                 std::size_t thinningResolution,
                 bool useSegmentedSkel,
                 bool equalTime = true); // If false, compute results for the same number of iterations, specified by "renderTimeMsOrIterationCount"

    void run();

private:
    bool initScene(const std::string& configName);

    void sceneThinning();

    struct Settings {
        FilePath m_FilePath; // Path to viewer settings file
        tinyxml2::XMLDocument m_Document; // XML document associated to viewer settings file
        tinyxml2::XMLElement* m_pRoot = nullptr; // Root of m_Document
        Vec2u m_WindowSize;
        Vec2u m_FramebufferSize;
        float m_fFramebufferRatio;

        Settings(const FilePath& filepath);
    };

    FilePath m_ViewerDirPath;

    Settings m_Settings;

    WindowManager m_WindowManager;
    GUI m_GUI;
    GLShaderManager m_ShaderManager;

    GLImageRenderer m_GLImageRenderer;

    // Scene
    ConfigManager m_ConfigManager;
    tinyxml2::XMLDocument m_ConfigDoc;

    FilePath m_SceneDocumentFilePath;
    tinyxml2::XMLDocument m_SceneDocument;
    Unique<Scene> m_pScene;

    ProjectiveCamera m_Camera;
    Vec2f m_ZNearFar; // Computed from the scene bounds

    bool m_bGUIHasFocus = false;

    GLScreenFramebuffer m_ScreenFramebuffer;
    float m_fGamma = 2.2f;

    std::size_t m_nVoxelGridRes = 128;
    bool m_bUseSegmentedSkeleton = false;
    TaskTimer m_ThinningTimer = TaskTimer({ "Voxelize",
                                "GetVoxelGridFromGPU",
                                "InverseToGetEmptySpace",
                                "ConvertToCubicalComplex",
                                "ComputeDistanceMap",
                                "ComputeOpeningMap",
                                "InitThinning",
                                "PerformThinning" }, 1u);

    bool m_bInitSceneFlag = false;
    PG15RendererManager m_RendererManager;
};

}
