#pragma once

#include <bonez/viewer/WindowManager.hpp>
#include <bonez/opengl/GLShaderManager.hpp>
#include <bonez/opengl/GLImageRenderer.hpp>
#include <bonez/parsing/parsing.hpp>

namespace BnZ {

class ResultsViewer {
    void load(const FilePath& path);

    void drawResultsDirGUI(const FilePath& relativePath, const FilePath& absolutePath);

    void drawGUI(const Image& image);

    void drawGUIOverlay(const Image& image);

    void setCurrentImage(const Shared<Image>& pImage);

    void checkCurrentImageValidity();

protected:
    FilePath m_ApplicationPath;
    FilePath m_ResultsDirPath;

    WindowManager m_WindowManager;
    GUI m_GUI;
    GLShaderManager m_ShaderManager;

    GLImageRenderer m_GLImageRenderer;

    bool m_bGUIHasFocus = false;
    float m_fGamma = 1.f;
    float m_fScale = 1.f;
    bool m_bCompareMode = false;
    bool m_bShowReference = false;
    bool m_bSumMode = false;

    Vec3f m_NRMSE = zero<Vec3f>();
    Vec3f m_MSE = zero<Vec3f>();
    Vec3f m_RMSE = zero<Vec3f>();

    Vec2u m_HoveredPixel = Vec2u(0);

    Shared<Image> m_pCurrentReferenceImage;
    Shared<Image> m_pCurrentImage;

    Framebuffer m_CurrentFramebuffer;
    uint32_t m_nChannelIdx = 0u;
    bool m_bDisplayFramebufferChannel = false;

    std::vector<std::vector<float>> m_RMSECurves;

    std::vector<uint32_t> m_ListOfInvalidPixels;
    std::vector<std::string> m_ListOfInvalidPixelsStr;
    uint32_t m_nCurrentInvalidPixel = 0u;

    Image m_SmallViewImage;
public:
    ResultsViewer(const FilePath& applicationPath,
                  const FilePath& resultsPath,
                  const Vec2u& windowSize);

    void run();
};

}
