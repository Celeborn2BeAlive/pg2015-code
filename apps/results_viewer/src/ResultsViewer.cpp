#include "ResultsViewer.hpp"

namespace BnZ {

ResultsViewer::ResultsViewer(const FilePath& applicationPath,
                             const FilePath& resultsPath,
                             const Vec2u& windowSize):
    m_ApplicationPath(applicationPath),
    m_ResultsDirPath(resultsPath),
    m_WindowManager(windowSize.x, windowSize.y, "ResultsViewer"),
    m_GUI(applicationPath.file(), m_WindowManager),
    m_ShaderManager(applicationPath.directory() + "shaders"),
    m_GLImageRenderer(m_ShaderManager),
    m_SmallViewImage(5, 5) {
}

void ResultsViewer::load(const FilePath& path) {
    if(path.ext() == "png") {
        auto pImg = BnZ::loadImage(path);
        pImg->flipY();

        setCurrentImage(pImg);
    } else if(path.ext() == "raw") {
        setCurrentImage(loadRawImage(path));
    } else if(path.ext() == "exr") {
        auto filename = path.file();
        filename = filename.substr(0, filename.size() - 4);
        if(stringEndsWith(filename, ".bnzframebuffer")) {
            std::clog << "Load EXR Framebuffer..." << std::endl;
            m_CurrentFramebuffer = loadEXRFramebuffer(path);
            m_nChannelIdx = 0u;
        } else {
            setCurrentImage(loadEXRImage(path));
        }
    } else if(path.ext() == "rmseFloat") {
        m_RMSECurves.emplace_back();
        loadArray(path, m_RMSECurves.back());
    }
}

void ResultsViewer::checkCurrentImageValidity() {
    m_ListOfInvalidPixels.clear();
    m_ListOfInvalidPixelsStr.clear();
    m_nCurrentInvalidPixel = 0u;
    for(auto i: range(m_pCurrentImage->getPixelCount())) {
        auto value = (*m_pCurrentImage)[i];
        if(isInvalidMeasurementEstimate(value)) {
            m_ListOfInvalidPixels.emplace_back(i);
            m_ListOfInvalidPixelsStr.emplace_back(toString(i));
        }
    }
}

void ResultsViewer::drawGUI(const Image& image) {
    if(auto window = m_GUI.addWindow("View parameters")) {
        m_GUI.addVarRW(BNZ_GUI_VAR(m_fGamma));
        m_GUI.addVarRW(BNZ_GUI_VAR(m_fScale));
        m_GUI.addButton("Check validity", [&]() {
            checkCurrentImageValidity();
        });
        m_GUI.addSeparator();
        m_GUI.addButton("Set as reference", [&]() {
            m_pCurrentReferenceImage = m_pCurrentImage;
        });
        m_GUI.addVarRW(BNZ_GUI_VAR(m_bCompareMode));
        m_GUI.addVarRW(BNZ_GUI_VAR(m_bShowReference));

        m_GUI.addValue(BNZ_GUI_VAR(m_NRMSE));
        m_GUI.addValue("NRMSE Mean", (m_NRMSE.x + m_NRMSE.y + m_NRMSE.z) / 3.f);
        m_GUI.addValue(BNZ_GUI_VAR(m_MSE));
        m_GUI.addValue("MSE Mean", (m_MSE.x + m_MSE.y + m_MSE.z) / 3.f);
        m_GUI.addValue(BNZ_GUI_VAR(m_RMSE));
        m_GUI.addValue("RMSE Mean", (m_RMSE.x + m_RMSE.y + m_RMSE.z) / 3.f);

        m_GUI.addSeparator();
        m_GUI.addVarRW(BNZ_GUI_VAR(m_bSumMode));
    }

    if(auto window = m_GUI.addWindow("Results directory")) {
        drawResultsDirGUI(m_ResultsDirPath, m_ResultsDirPath);
    }

    if(auto window = m_GUI.addWindow("Curves")) {
        m_GUI.addButton("Reset", [&]() {
            m_RMSECurves.clear();
        });
        for(const auto& curve: m_RMSECurves) {
            ImGui::PlotLines("NRMSE", curve.data(), curve.size(), 0, nullptr, 0, 1, ImVec2(512, 128));
        }
    }

    if(auto window = m_GUI.addWindow("Invalid pixels")) {
        if(!m_ListOfInvalidPixels.empty()) {
            m_GUI.addValue("Count", m_ListOfInvalidPixels.size());
            m_GUI.addCombo("pixel", m_nCurrentInvalidPixel, m_ListOfInvalidPixels.size(), [&](uint32_t i) {
               return m_ListOfInvalidPixelsStr[i].c_str();
            });
            auto pixelIndex = m_ListOfInvalidPixels[m_nCurrentInvalidPixel];
            m_GUI.addValue("PixelIndex", pixelIndex);
            auto pixel = getPixel(pixelIndex, m_pCurrentImage->getSize());
            m_GUI.addValue("PixelCoords", pixel);
            m_GUI.addValue("PixelValue", (*m_pCurrentImage)[pixelIndex]);
        }
    }

    if(auto window = m_GUI.addWindow("CurrentFramebuffer")) {
        m_GUI.addValue("ChannelCount", m_CurrentFramebuffer.getChannelCount());
        m_GUI.addValue("Width", m_CurrentFramebuffer.getWidth());
        m_GUI.addValue("Height", m_CurrentFramebuffer.getHeight());
        m_GUI.addVarRW(BNZ_GUI_VAR(m_bDisplayFramebufferChannel));

        if(m_CurrentFramebuffer.getChannelCount()) {
            m_GUI.addCombo("Channel", m_nChannelIdx, m_CurrentFramebuffer.getChannelCount(), [&](uint32_t channelIdx) {
                return m_CurrentFramebuffer.getChannelName(channelIdx).c_str();
            });
        }

        m_GUI.addButton("Export as EXR", [&]() {
            storeEXRImage("lol.exr", m_CurrentFramebuffer.getChannel(m_nChannelIdx));
        });
    }

    drawGUIOverlay(image);
}

void ResultsViewer::setCurrentImage(const Shared<Image>& pImage) {
    if(m_bSumMode) {
        for(auto i: range(m_pCurrentImage->getPixelCount())) {
            (*m_pCurrentImage)[i] += Vec4f(Vec3f((*pImage)[i]), 0.f);
        }
    } else {
        m_pCurrentImage = pImage;
    }

    checkCurrentImageValidity();
}

void ResultsViewer::drawResultsDirGUI(const FilePath& relativePath, const FilePath& absolutePath) {
    if(isDirectory(absolutePath)) {
        if (ImGui::TreeNode(relativePath.c_str()))
        {
            Directory dir(absolutePath);

            for(const auto& child: dir.files()) {
                auto absoluteChildPath = absolutePath + child;
                drawResultsDirGUI(child, absoluteChildPath);
            }
            ImGui::TreePop();
        }
    } else {
        if (ImGui::SmallButton(relativePath.c_str())) {
            load(absolutePath);
        }
    }
}

void ResultsViewer::drawGUIOverlay(const Image& image) {
    if(auto window = m_GUI.addWindow("Displayed Image")) {
        ImGui::Text("Mouse Position: (%.1f,%.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);

        m_GUI.addValue(BNZ_GUI_VAR(m_HoveredPixel));
        if(!image.empty()) {
            auto value = image(m_HoveredPixel.x, m_HoveredPixel.y);
            m_GUI.addValue("Value", value);
            auto normalizedValue = value / value.w;
            m_GUI.addValue("Normalized Value", value / value.w);
            auto color = normalizedValue;
            ImGui::Color("Color", ImVec4(color.x, color.y, color.z, color.w));

            Vec3f sum, mean, variance;
            computeImageStatistics(image, sum, mean, variance);
            m_GUI.addValue("Sum", sum);
            m_GUI.addValue("Mean", mean);
            m_GUI.addValue("Variance", variance);
        } else {
            m_GUI.addValue("Value", Vec4f(0.f));
            m_GUI.addValue("Normalized Value", Vec4f(0.f));
            ImGui::Color("Color", ImVec4(0, 0, 0, 0));
        }
    }
}


void ResultsViewer::run() {
    while (m_WindowManager.isOpen()) {
        Image displayImage;

        auto compareMode = [&](const Image& image) {
            displayImage = computeSquareErrorImage(*m_pCurrentReferenceImage,
                                                   image,
                                                   m_NRMSE);
        };

        if(m_bDisplayFramebufferChannel) {
            if(m_bCompareMode && m_pCurrentReferenceImage) {
                compareMode(m_CurrentFramebuffer.getChannel(m_nChannelIdx));
            } else {
                displayImage = m_CurrentFramebuffer.getChannel(m_nChannelIdx);
            }
        } else if(m_pCurrentImage) {
            if(m_bShowReference) {
                displayImage = *m_pCurrentReferenceImage;
            } else if(m_bCompareMode && m_pCurrentImage && m_pCurrentReferenceImage) {
                compareMode(*m_pCurrentImage);
            } else {
                displayImage = *m_pCurrentImage;
            }
        }

        if(!displayImage.empty()) {
            displayImage.scale(m_fScale);

            Vec3f sum, mean, variance;
            computeImageStatistics(displayImage, sum, mean, variance);

            m_MSE = mean;
            m_RMSE = sqrt(mean);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        m_WindowManager.handleEvents();
        m_GUI.startFrame();

        drawGUI(displayImage);

        if(!displayImage.empty()) {
            auto viewport = Vec4u((m_WindowManager.getSize() - m_pCurrentImage->getSize()) / 2u,
                                  m_pCurrentImage->getSize());
            glViewport(viewport.x, viewport.y, viewport.z, viewport.w);

            m_GLImageRenderer.drawImage(m_fGamma, displayImage, true, false);

            auto smallViewOrigin = Vec2i(m_HoveredPixel) - Vec2i(m_SmallViewImage.getSize()) / 2;
            for(auto y : range(m_SmallViewImage.getHeight())) {
                for(auto x : range(m_SmallViewImage.getWidth())) {
                    if(displayImage.contains(smallViewOrigin.x + x, smallViewOrigin.y + y)) {
                        m_SmallViewImage(x, y) = displayImage(smallViewOrigin.x + x, smallViewOrigin.y + y);
                    } else {
                        m_SmallViewImage(x, y) = Vec4f(0.f);
                    }
                }
            }

            auto smallImageViewport = Vec4u(0, 0, m_SmallViewImage.getWidth() * 32, m_SmallViewImage.getHeight() * 32);

            glViewport(smallImageViewport.x, smallImageViewport.y, smallImageViewport.z, smallImageViewport.w);
            m_GLImageRenderer.drawImage(m_fGamma, m_SmallViewImage, true, false, GL_NEAREST);

            if(m_WindowManager.hasClicked()) {
                auto pixel = m_WindowManager.getCursorPosition(Vec2u(viewport));
                if(pixel.x >= 0 && pixel.y >= 0 &&
                        pixel.x < viewport.z && pixel.y < viewport.w) {
                    m_HoveredPixel = Vec2u(pixel);
                } else {
                    pixel = m_WindowManager.getCursorPosition(Vec2u(smallImageViewport));
                    if(pixel.x >= 0 && pixel.y >= 0 &&
                            pixel.x < smallImageViewport.z && pixel.y < smallImageViewport.w) {
                        auto bigPixel = pixel / 32;
                        auto truePixel = smallViewOrigin + bigPixel;
                        if(displayImage.contains(truePixel.x, truePixel.y)) {
                            m_HoveredPixel = truePixel;
                        }
                    }
                }
            }
        }

        auto size = m_WindowManager.getSize();
        glViewport(0, 0, (int)size.x, (int)size.y);
        m_bGUIHasFocus = m_GUI.draw();

        m_WindowManager.swapBuffers();
    }
}

}
