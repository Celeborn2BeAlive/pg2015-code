#include "PG2015Viewer.hpp"

#include <bonez/rendering/renderers/SkeletonMappingRenderer.hpp>

#include <bonez/voxskel/GLVoxelizerTripiana2009.hpp>
#include <bonez/voxskel/discrete_functions.hpp>
#include <bonez/voxskel/ThinningProcessDGCI2013_2.hpp>

namespace BnZ {

PG2015Viewer::PG2015Viewer(const FilePath& applicationPath,
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
                           bool equalTime):
    m_ViewerDirPath(viewerFilePath.directory()),
    m_Settings(viewerFilePath),
    m_WindowManager(m_Settings.m_WindowSize.x, m_Settings.m_WindowSize.y,
                    "PG2015Viewer"),
    m_GUI(applicationPath.file(), m_WindowManager),
    m_ShaderManager(applicationPath.directory() + "shaders"),
    m_GLImageRenderer(m_ShaderManager),
    m_nVoxelGridRes(thinningResolution),
    m_bUseSegmentedSkeleton(useSegmentedSkel),
    m_bInitSceneFlag(initScene(configName)),
    m_RendererManager(*m_pScene,
                      m_Camera,
                      m_Settings.m_FramebufferSize,
                      referenceImagePath,
                      resultPath,
                      m_ConfigDoc,
                      m_SceneDocument,
                      m_fGamma,
                      renderTimeMsOrIterationCount,
                      maxPathDepth,
                      resamplingPathCount,
                      icBPTSettings,
                      skelBPTSettings,
                      equalTime) {

    m_ScreenFramebuffer.init(m_Settings.m_FramebufferSize);

    tinyxml2::XMLDocument thinningReportDocument;
    auto pRoot = thinningReportDocument.NewElement("ThinningReport");
    thinningReportDocument.InsertEndChild(pRoot);

    setChildAttribute(*pRoot, "NodeCount", m_pScene->getCurvSkeleton()->size());
    setChildAttribute(*pRoot, "InitialResolution", m_nVoxelGridRes);
    setChildAttribute(*pRoot, "ReducedResolution", m_pScene->getCurvSkeleton()->getGrid().resolution());
    setChildAttribute(*pRoot, "UseSegmentedSkeleton", m_bUseSegmentedSkeleton);
    setChildAttribute(*pRoot, "Time", m_ThinningTimer);

    thinningReportDocument.SaveFile((m_RendererManager.getResultPath() + "thinning.report.bnz.xml").c_str());
}

bool PG2015Viewer::initScene(const std::string& configName) {
    auto pScene = m_Settings.m_pRoot->FirstChildElement("Scene");
    if(!pScene) {
        throw std::runtime_error("No Scene  tag found in viewer settings file");
    }

    m_SceneDocumentFilePath = m_ViewerDirPath + pScene->Attribute("path");

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

    // Init ConfigManager
    auto pConfigs = m_SceneDocument.RootElement()->FirstChildElement("Configs");
    if(!pConfigs || ! pConfigs->Attribute("path")) {
        throw std::runtime_error("No Configs path specified in scene settings");
    }
    auto configPath = m_SceneDocumentFilePath.directory() + pConfigs->Attribute("path");
    m_ConfigManager.setPath(configPath);

    m_ConfigManager.loadConfig(configName, m_Camera, *m_pScene,
                             m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                             m_ZNearFar.x, m_ZNearFar.y, m_ConfigDoc);

    sceneThinning();

    return true;
}

void PG2015Viewer::sceneThinning() {
    TIMED_FUNC(timerObj);

    auto pLogger = el::Loggers::getLogger("SceneThinning");

    GLVoxelizerTripiana2009 m_GLVoxelizerTripiana2009(m_ShaderManager);
    ThinningProcessDGCI2013_2 m_ThinningProcess;
    Mat4f m_GridToWorldMatrix;
    GLVoxelFramebuffer m_GLVoxelFramebuffer;
    VoxelGrid m_VoxelGrid;
    CubicalComplex3D m_EmptySpaceCubicalComplex;
    CubicalComplex3D m_SkeletonCubicalComplex;
    Grid3D<uint32_t> m_EmptySpaceDistanceMap;
    Grid3D<uint32_t> m_EmptySpaceOpeningMap;
    Grid3D<Vec3u> m_EmptySpaceOpeningCenterMap;

    GLScene glScene(m_pScene->getGeometry());

    {
        TIMED_SCOPE(timerBlkObj, "Voxelization");

        auto timer = m_ThinningTimer.start(0);
        pLogger->info("Voxelizing the scene with resolution = %v", m_nVoxelGridRes);

        m_GLVoxelizerTripiana2009.initGLState(m_nVoxelGridRes, m_pScene->getBBox(),
            m_GLVoxelFramebuffer, m_GridToWorldMatrix);
        glScene.render();
        m_GLVoxelizerTripiana2009.restoreGLState();
    }

    {
        TIMED_SCOPE(timerBlkObj, "GetVoxelGrid");

        auto timer = m_ThinningTimer.start(1);
        pLogger->info("Get scene voxel grid from GPU");

        m_VoxelGrid = getVoxelGrid(m_GLVoxelFramebuffer);
        m_VoxelGrid = deleteEmptyBorder(m_VoxelGrid, m_GridToWorldMatrix, m_GridToWorldMatrix);

        pLogger->info("Resolution of the new grid = %v %v %v", m_VoxelGrid.width(), m_VoxelGrid.height(), m_VoxelGrid.depth());
    }

    {
        TIMED_SCOPE(timerBlkObj, "InverseVoxelGrid");

        auto timer = m_ThinningTimer.start(2);
        pLogger->info("Inverse scene voxel grid to get empty space voxel grid");
        m_VoxelGrid = inverse(m_VoxelGrid);
    }

    {
        TIMED_SCOPE(timerBlkObj, "ConvertToCC3D");

        auto timer = m_ThinningTimer.start(3);
        pLogger->info("Convert empty space voxel grid to CC3D");
        m_EmptySpaceCubicalComplex = getCubicalComplex(m_VoxelGrid);
    }

    {
        TIMED_SCOPE(timerBlkObj, "ComputeDistanceMap");

        auto timer = m_ThinningTimer.start(4);
        pLogger->info("Compute Distance Map");
        m_EmptySpaceDistanceMap = computeDistanceMap26(m_EmptySpaceCubicalComplex, CubicalComplex3D::isInObject, true);
    }

    {
        TIMED_SCOPE(timerBlkObj, "ComputeOpeningMap");

        auto timer = m_ThinningTimer.start(5);
        pLogger->info("Compute Opening Map");
        m_EmptySpaceOpeningMap = parallelComputeOpeningMap26(m_EmptySpaceDistanceMap, m_EmptySpaceOpeningCenterMap);
    }

    {
        m_SkeletonCubicalComplex = m_EmptySpaceCubicalComplex;

        {
            TIMED_SCOPE(timerBlkObj, "InitThinning");

            auto timer = m_ThinningTimer.start(6);
            pLogger->info("Initialize the thinning process");
            m_ThinningProcess.init(m_SkeletonCubicalComplex, &m_EmptySpaceDistanceMap, &m_EmptySpaceOpeningMap);
        }

        {
            TIMED_SCOPE(timerBlkObj, "RunThinning");

            auto timer = m_ThinningTimer.start(7);
            pLogger->info("Run the thinning process");
            m_ThinningProcess.directionalCollapse();
        }
    }


    auto skeleton = getCurvilinearSkeleton(m_SkeletonCubicalComplex, m_EmptySpaceCubicalComplex,
                                           m_EmptySpaceDistanceMap, m_EmptySpaceOpeningMap, m_GridToWorldMatrix);
    if(!m_bUseSegmentedSkeleton) {
        m_pScene->setCurvSkeleton(skeleton);
    } else {
        auto segSkeleton = computeMaxballBasedSegmentedSkeleton(skeleton);
        m_pScene->setCurvSkeleton(segSkeleton);
    }
}

void PG2015Viewer::run() {
    std::atomic<bool> done { false };

    std::thread renderThread([&]() {
        while(!done) {
            if(m_RendererManager.render()) {
                done = true;
            }
        }
    });

    auto windowSize = m_WindowManager.getSize();
    auto finalViewport =
            Vec4f(0.5 * (windowSize.x - m_Settings.m_FramebufferSize.x),
                  0.5 * (windowSize.y - m_Settings.m_FramebufferSize.y),
                  m_Settings.m_FramebufferSize);

    glDisable(GL_DEPTH_TEST);

    uint32_t nFramebufferChannelIdx = 0;

    while (m_WindowManager.isOpen() && !done) {
        auto startTime = m_WindowManager.getSeconds();

        m_WindowManager.handleEvents();

        m_GUI.startFrame();

        m_RendererManager.drawGUI(m_GUI);

        auto& currentFramebuffer = m_RendererManager.getCurrentFramebuffer();
        if(auto window = m_GUI.addWindow("Viewer")) {
            m_GUI.addVarRW(BNZ_GUI_VAR(m_fGamma));

            m_GUI.addCombo("Framebuffer Channel", nFramebufferChannelIdx, currentFramebuffer.getChannelCount(), [&](uint32_t channelIdx) {
                return currentFramebuffer.getChannelName(channelIdx).c_str();
            });
            nFramebufferChannelIdx = clamp(nFramebufferChannelIdx, uint32_t(0), uint32_t(currentFramebuffer.getChannelCount()) - 1);
        }

        m_ScreenFramebuffer.bindForDrawing();

        glViewport(0, 0, m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y);
        glClear(GL_COLOR_BUFFER_BIT);

        m_GLImageRenderer.drawFramebuffer(m_fGamma, currentFramebuffer, nFramebufferChannelIdx);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        m_ScreenFramebuffer.bindForReading();
        m_ScreenFramebuffer.setReadBuffer(0);

        glBlitFramebuffer(0, 0, m_Settings.m_FramebufferSize.x, m_Settings.m_FramebufferSize.y,
                          finalViewport.x, finalViewport.y, finalViewport.x + finalViewport.z, finalViewport.y + finalViewport.w,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        glViewport(0, 0, windowSize.x, windowSize.y);
        m_bGUIHasFocus = m_GUI.draw();

        m_WindowManager.swapBuffers();
    }

    done = true;

    LOG(INFO) << "Wait for the RendererManager to end the current iteration...";
    renderThread.join();
}

PG2015Viewer::Settings::Settings(const FilePath& filepath):
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

    auto pFramebuffer = m_pRoot->FirstChildElement("Framebuffer");
    if(!getAttribute(*pFramebuffer, "width", m_FramebufferSize.x) ||
            !getAttribute(*pFramebuffer, "height", m_FramebufferSize.y)) {
        throw std::runtime_error("Invalid viewer settings file format (no width/height params for the framebuffer)");
    }
    m_fFramebufferRatio = float(m_FramebufferSize.x) / m_FramebufferSize.y;
}

}
