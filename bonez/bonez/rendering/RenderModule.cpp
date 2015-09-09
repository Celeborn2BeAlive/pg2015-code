#include "RenderModule.hpp"
#include <bonez/utils/ColorMap.hpp>

namespace BnZ {

static const std::string RES_IMAGE_EXT = "png";
static const std::string RES_EXR_EXT = "exr";
static const std::string RES_STATS_DIR = "stats";

struct RenderStatistics {
    // All timings are in milliseconds
    Microseconds totalTime;
    Microseconds initTime;
    Microseconds renderTime;

    uint32_t iterCount;

    // For each spp, contains the TOTAL rendering time to reach it
    // (ie. initFrameTime + processing time of all spp):
    std::vector<float> processingTimes;

    std::vector<Vec3f> nrmse; // Relative root mean square error for each spp and each channel
    std::vector<float> nrmseFloat; // Relative root mean square error for each spp

    std::vector<Vec3f> rmse;
    std::vector<float> rmseFloat;

    std::vector<Vec3f> absError;
    std::vector<float> absErrorFloat;
};

static void storeFramebuffer(int resultIndex,
                             const FilePath& pngDir,
                             const FilePath& exrDir,
                             const FilePath& baseName,
                             float gamma,
                             const Framebuffer& framebuffer) {
    // Create output directories in case they don't exist
    createDirectory(pngDir.str());
    createDirectory(exrDir.str());

    // Path of primary output image files
    FilePath pngFile = pngDir + baseName.addExt("." + RES_IMAGE_EXT);
    FilePath exrFile = exrDir + baseName.addExt("." + RES_EXR_EXT);

    // Make a copy of the image
    Image copy = framebuffer.getChannel(0);
    // Store the EXR image "as is"
    storeEXRImage(exrFile.str(), copy);

    // Post-process copy of image and store PNG file
    copy.flipY();
    copy.divideByAlpha();
    copy.applyGamma(gamma);
    storeImage(pngFile.str(), copy);

    // Store complete framebuffer as a single EXR multi layer image
    auto exrFramebufferFilePath = exrDir + baseName.addExt(".bnzframebuffer." + RES_EXR_EXT);
    storeEXRFramebuffer(exrFramebufferFilePath.str(), framebuffer);

    // Store complete framebuffer as PNG indivual files in a dediacted subdirectory
    auto pngFramebufferDirPath = pngDir + "framebuffers/";
    createDirectory(pngFramebufferDirPath.str());
    if(resultIndex >= 0) {
        pngFramebufferDirPath = pngFramebufferDirPath + toString3(resultIndex); // Split different results of the same batch in different subdirectories
    }
    createDirectory(pngFramebufferDirPath.str());

    // Store each channel of the framebuffer
    for(auto i = 0u; i < framebuffer.getChannelCount(); ++i) {
        auto name = framebuffer.getChannelName(i);
        // Prefix by the index of the channel
        FilePath pngFile = pngFramebufferDirPath + FilePath(toString3(i)).addExt("_" + name + "." + RES_IMAGE_EXT);

        auto copy = framebuffer.getChannel(i);

        copy.flipY();
        copy.divideByAlpha();

        copy.applyGamma(gamma);
        storeImage(pngFile.str(), copy);
    }
}

void RenderModule::storeResult(const FilePath& resultDir, uint32_t index,
                               const RenderStatistics& stats, float gamma, const Framebuffer& framebuffer) {
    FilePath baseName(toString3(index));

    FilePath pngDir = resultDir + RES_IMAGE_EXT;
    FilePath exrDir = resultDir + RES_EXR_EXT;
    FilePath statsDir = resultDir + RES_STATS_DIR;

    storeFramebuffer(index, pngDir, exrDir, baseName, gamma, framebuffer);

    createDirectory(statsDir.str());

    storeArray(statsDir + baseName.addExt(".nrmse"), stats.nrmse);
    storeArray(statsDir + baseName.addExt(".nrmseFloat"), stats.nrmseFloat);
    storeArray(statsDir + baseName.addExt(".rmse"), stats.rmse);
    storeArray(statsDir + baseName.addExt(".rmseFloat"), stats.rmseFloat);
    storeArray(statsDir + baseName.addExt(".absError"), stats.absError);
    storeArray(statsDir + baseName.addExt(".absErrorFloat"), stats.absErrorFloat);
    storeArray(statsDir + baseName.addExt(".processingTimes"), stats.processingTimes);
}

static const std::string RendererString = "Renderer";

bool RenderModule::runCompareRenderGroup(
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
        const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters) {
    auto pNewSettings = pGroupElement.GetDocument()->NewElement("Settings");

    // Copy group settings
    for(auto pAttr = pGroupElement.FirstChildElement();
        pAttr;
        pAttr = pAttr->NextSiblingElement()) {
        if(RendererString != pAttr->Name()) {
            pNewSettings->InsertEndChild(cloneXMLNode(pAttr, pNewSettings->GetDocument()));
        }
    }

    // Copy existing settings
    if(pHeritedSettings) {
        for(auto pAttr = pHeritedSettings->FirstChildElement();
            pAttr;
            pAttr = pAttr->NextSiblingElement()) {
            if(RendererString != pAttr->Name()) {
                pNewSettings->InsertEndChild(cloneXMLNode(pAttr, pNewSettings->GetDocument()));
            }
        }
    }

    for(auto pChildElement = pGroupElement.FirstChildElement();
        pChildElement; pChildElement = pChildElement->NextSiblingElement()) {
        auto name = std::string(pChildElement->Name());
        if(name == "RenderGroup") {
            if(!runCompareRenderGroup(resultPath,
                                  *pChildElement,
                                  pNewSettings,
                                  scene,
                                  camera,
                                  iterCount,
                                  processingTime,
                                  minRMSE,
                                  renderCount,
                                  referenceImage,
                                  callback,
                                  handleGlobalPreprocessParameters)) {
                return false;
            }
        } else if(name == "Renderer") {
            auto pRendererSettings = pGroupElement.GetDocument()->NewElement("Renderer");

            // copy herited settings
            for(auto pAttr = pChildElement->FirstChildElement();
                pAttr;
                pAttr = pAttr->NextSiblingElement()) {
                if(RendererString != pAttr->Name()) {
                    pRendererSettings->InsertEndChild(cloneXMLNode(pAttr, pNewSettings->GetDocument()));
                }
            }

            // copy herited settings
            for(auto pAttr = pNewSettings->FirstChildElement();
                pAttr;
                pAttr = pAttr->NextSiblingElement()) {
                if(RendererString != pAttr->Name()) {
                    pRendererSettings->InsertEndChild(cloneXMLNode(pAttr, pNewSettings->GetDocument()));
                }
            }

            pRendererSettings->SetAttribute("name", pChildElement->Attribute("name"));

            if(!renderCompare(resultPath,
                          *pRendererSettings,
                          scene,
                          camera,
                          iterCount,
                          processingTime,
                          minRMSE,
                          renderCount,
                          referenceImage,
                          callback,
                          handleGlobalPreprocessParameters)) {
                return false;
            }
            ++renderCount;
        }
    }
    return true;
}

bool RenderModule::renderCompare(
        const FilePath& resultPath,
        const tinyxml2::XMLElement& rendererSettings,
        const Scene& scene,
        const Camera& camera,
        int iterCount,
        float processingTimeMs,
        float minRMSE,
        uint32_t index,
        const Image& referenceImage,
        const std::function<bool()>& callback,
        const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters)
{
    bool done = false;

    clearCPUFramebuffer();
    auto& framebuffer = m_CPUFramebuffer;

    auto reportsPath = resultPath + "reports";
    createDirectory(reportsPath);
    auto reportDocumentPath = reportsPath + (toString3(index) + ".report.bnz.xml");
    tinyxml2::XMLDocument reportDocument;

    auto pReport = reportDocument.NewElement("Result");
    auto pRendererStats = reportDocument.NewElement("RendererStats");
    auto pRendererSettings = reportDocument.NewElement("Renderer");
    auto pGlobalPreprocessOutput = reportDocument.NewElement("GlobalPreprocessOutput");
    setAttribute(*pRendererSettings, "name", rendererSettings.Attribute("name"));

    RenderStatistics stats;

    std::clog << "GPU Memory1 = " << getGPUMemoryInfoCurrentAvailable() << std::endl;
    {
        auto pGlobalPreprocessParameters = rendererSettings.FirstChildElement("GlobalPreprocessParameters");
        if(pGlobalPreprocessParameters) {
            handleGlobalPreprocessParameters(*pGlobalPreprocessParameters, *pGlobalPreprocessOutput);
        }

        auto pRenderer = m_RendererManager.createRenderer(rendererSettings.Attribute("name"));

        pRenderer->setStatisticsOutput(pRendererStats);

        std::clog << "Render " << index << "..." << std::endl;

        std::clog << "Renderer::init" << "..." << std::endl;
        {
            Timer timer(true);

            pRenderer->init(scene, camera, framebuffer, &rendererSettings);

            stats.initTime = timer.getMicroEllapsedTime();
        }
        std::clog << "Done." << std::endl;

        stats.renderTime = Microseconds { 0 };
        stats.iterCount = 0u;

        float nrmseFloat = std::numeric_limits<float>::max();
        while((iterCount < 0 && us2ms(stats.renderTime) < processingTimeMs && nrmseFloat > minRMSE) || (iterCount >= 0 && iterCount--)) {
            {
                Timer timer;

                pRenderer->render();

                stats.renderTime += timer.getMicroEllapsedTime();
            }

            ++stats.iterCount;

            auto nrmse = computeNormalizedRootMeanSquaredError(referenceImage, framebuffer.getChannel(0));
            nrmseFloat = (nrmse.r + nrmse.g + nrmse.b) / 3.f;

            auto rmse = computeRootMeanSquaredError(referenceImage, framebuffer.getChannel(0));
            auto rmseFloat = (rmse.r + rmse.g + rmse.b) / 3.f;

            auto absError = computeMeanAbsoluteError(referenceImage, framebuffer.getChannel(0));
            auto absErrorFloat = (absError.r + absError.g + absError.b) / 3.f;

            std::clog << "NRMSE = " << nrmse << " ; Mean = " << nrmseFloat << std::endl;
            std::clog << "RMSE = " << rmse << " ; Mean = " << rmseFloat << std::endl;
            std::clog << "MAE = " << absError << " ; Mean = " << absErrorFloat << std::endl;
            std::clog << "renderTime = " << us2ms(stats.renderTime) << " / " << processingTimeMs << std::endl;
            std::clog << "iterationCount = " << stats.iterCount << " remaining = " << iterCount << std::endl;

            stats.processingTimes.emplace_back(us2ms(stats.renderTime));
            stats.nrmse.emplace_back(nrmse);
            stats.nrmseFloat.emplace_back(nrmseFloat);
            stats.rmse.emplace_back(rmse);
            stats.rmseFloat.emplace_back(rmseFloat);
            stats.absError.emplace_back(absError);
            stats.absErrorFloat.emplace_back(absErrorFloat);

            if(!callback()) {
                done = true;
                break;
            }
        }

        std::clog << "Store images" << std::endl;

        storeResult(resultPath, index, stats, m_fGamma, framebuffer);

        std::clog << "Done." << std::endl;

        std::clog << "Store statistics" << std::endl;

        pRenderer->storeStatistics();

        std::clog << "Done." << std::endl;

        std::clog << "Store settings" << std::endl;

        pRenderer->storeSettings(*pRendererSettings);

        std::clog << "Done." << std::endl;
    }
    std::clog << "GPU Memory2 = " << getGPUMemoryInfoCurrentAvailable() << std::endl;

    stats.totalTime = stats.initTime + stats.renderTime;

    reportDocument.InsertEndChild(pReport);

    setAttribute(*pReport, "Index", index);
    setChildAttribute(*pReport, "Gamma", m_fGamma);

    setChildAttribute(*pReport, "TotalTime", stats.totalTime);
    setChildAttribute(*pReport, "InitTime", stats.initTime);
    setChildAttribute(*pReport, "RenderTime", stats.renderTime);
    setChildAttribute(*pReport, "NRMSE", stats.nrmseFloat.back());
    setChildAttribute(*pReport, "RMSE", stats.rmseFloat.back());
    setChildAttribute(*pReport, "MAE", stats.absErrorFloat.back());
    setChildAttribute(*pReport, "IterationCount", stats.iterCount);

    pReport->InsertEndChild(pGlobalPreprocessOutput);

    pReport->InsertEndChild(pRendererStats);

    pReport->InsertEndChild(pRendererSettings);

    reportDocument.SaveFile(reportDocumentPath.c_str());

    return !done;
}

static const char* RENDER_XML_FILENAME = "render.bnz.xml";

bool RenderModule::renderReference(
        tinyxml2::XMLDocument& configDocument,
        tinyxml2::XMLDocument& sceneDocument,
        const tinyxml2::XMLElement* pReferenceSettings,
        const FilePath& resultDirPath,
        const Scene& scene,
        const Camera& camera,
        const std::function<bool()>& frameCallback,
        const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters,
        Shared<Image>& referenceImage) {
    static const char* REFERENCE_DIRECTORY = "reference";
    static const char* REFERENCE_XML_FILE = "reference.bnz.xml";
    static const char* REFERENCE_IMAGE_FILE = "reference";
    static const char* REFERENCE_EXR_FILE = "reference.exr";
    static const char* REFERENCE_EXRFRAMEBUFFER_FILE = "reference.bnzframebuffer.exr";
    static const char* REFERENCE_XML_CONFIG_FILE  = "config.bnz.xml";
    static const char* REFERENCE_XML_SCENE_FILE  = "scene.bnz.xml";

    FilePath referenceDirPath = resultDirPath + REFERENCE_DIRECTORY;
    createDirectory(referenceDirPath);

    bool done = false;
    if(pReferenceSettings) {
        auto pRendererSettings = pReferenceSettings->FirstChildElement("Renderer");
        if(pRendererSettings) {
            FilePath referencePath = referenceDirPath + REFERENCE_XML_FILE;
            tinyxml2::XMLDocument referenceDoc;

            bool forceCompute = true;
            if((getAttribute(*pReferenceSettings, "forceCompute", forceCompute) && forceCompute) || !exists(referencePath)) {
                std::clog << "Render the reference..." << std::endl;

                uint32_t currentIterCount = 0u;
                Microseconds renderTime { 0 };

                if(exists(referencePath)) {
                    if(tinyxml2::XML_NO_ERROR == referenceDoc.LoadFile(referencePath.c_str())) {
                        auto pRoot = referenceDoc.RootElement();

                        getChildAttribute(*pRoot, "Gamma", m_fGamma);
                        getChildAttribute(*pRoot, "EllapsedTime", renderTime);
                        getChildAttribute(*pRoot, "IterationCount", currentIterCount);

                        pRendererSettings = pRoot->FirstChildElement("Renderer");
                    } else {
                        std::cerr << "Invalid reference file found. Switch to a new one." << std::endl;
                    }
                } else {
                    getAttribute(*pReferenceSettings, "gamma", m_fGamma);
                }

                uint32_t iterCount = 1u;
                getAttribute(*pReferenceSettings, "iterCount", iterCount);
                auto rendererName = pRendererSettings->Attribute("name");
                auto pRenderer = m_RendererManager.createRenderer(rendererName);

                clearCPUFramebuffer();

                Microseconds totalTime { 0 };

                {
                    Timer timer;
                    pRenderer->init(scene, camera, m_CPUFramebuffer, pRendererSettings);
                    totalTime += timer.getMicroEllapsedTime();
                }

                if(exists(referencePath)) {
                    std::clog << "Load existing framebuffer" << std::endl;
                    auto filePath = referenceDirPath + REFERENCE_EXRFRAMEBUFFER_FILE;
                    loadEXRFramebuffer(filePath, m_CPUFramebuffer);
                    std::clog << "Done." << std::endl;
                }

                for(auto i = 0u; i < iterCount; ++i) {
                    Timer timer;
                    pRenderer->render();
                    totalTime += timer.getMicroEllapsedTime();

                    ++currentIterCount;
                    std::clog << "Frame " << currentIterCount << " rendered." << std::endl;

                    if(!frameCallback()) {
                        done = true;
                        break;
                    }
                }

                renderTime += totalTime;

                std::clog << "Store framebuffer" << std::endl;
                storeFramebuffer(-1, referenceDirPath, referenceDirPath, REFERENCE_IMAGE_FILE, m_fGamma, m_CPUFramebuffer);
                std::clog << "Done." << std::endl;

                std::clog << "Store XML information" << std::endl;

                tinyxml2::XMLDocument referenceDoc;
                auto pResult = referenceDoc.NewElement("Reference");
                referenceDoc.InsertFirstChild(pResult);

                setChildAttribute(*pResult, "Gamma", m_fGamma);
                setChildAttribute(*pResult, "EllapsedTime", renderTime);
                setChildAttribute(*pResult, "IterationCount", currentIterCount);

                auto pRendererSettings = referenceDoc.NewElement("Renderer");
                pRenderer->storeSettings(*pRendererSettings);
                setAttribute(*pRendererSettings, "name", rendererName);
                pResult->InsertEndChild(pRendererSettings);

                referenceDoc.SaveFile(referencePath.c_str());
                configDocument.SaveFile((referenceDirPath + REFERENCE_XML_CONFIG_FILE).c_str());
                sceneDocument.SaveFile((referenceDirPath + REFERENCE_XML_SCENE_FILE).c_str());

                std::clog << "Done." << std::endl;
            }
        }
    }

    referenceImage = loadEXRImage(referenceDirPath + REFERENCE_EXR_FILE);
    return !done;
}

bool RenderModule::renderConfig(
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
        const std::function<void(const tinyxml2::XMLElement& input, tinyxml2::XMLElement& output)>& handleGlobalPreprocessParameters) {
    tinyxml2::XMLDocument configDoc;

    ProjectiveCamera camera;
    FilePath path = configManager.loadConfig(configName, camera, scene,
                                             m_CPUFramebuffer.getWidth(), m_CPUFramebuffer.getHeight(),
                                             zNear, zFar, configDoc);

    auto resultPath = resultsDirPath + configName;
    auto sessionPath = resultPath + sessionName;
    createDirectory(resultsDirPath);
    createDirectory(resultPath);
    createDirectory(sessionPath);

    Shared<Image> referenceImage;

    if(!renderReference(configDoc, sceneDocument, pReferenceElement, resultPath,
                        scene, camera, callback, handleGlobalPreprocessParameters,
                        referenceImage)) {
        return false;
    }
    if(!referenceImage) {
        std::cerr << "No reference" << std::endl;
        return false;
    }

    if(pRenderersDocument) {
        auto pRenders = pRenderersDocument->RootElement();
        if(!pRenders) {
            std::clog << "Invalide file format (no root element in " << RENDER_XML_FILENAME << ")" << std::endl;
            return false;
        }

        auto resultPrefix = getDateString();
        resultPath = sessionPath + resultPrefix;
        createDirectory(resultPath);

        configDoc.SaveFile((resultPath + "config.bnz.xml").c_str());
        sceneDocument.SaveFile((resultPath + "scene.bnz.xml").c_str());

        pRenderersDocument->SaveFile((resultPath + RENDER_XML_FILENAME).c_str());

        getAttribute(*pRenders, "gamma", m_fGamma);

        float processingTime = 0;
        getAttribute(*pRenders, "processingTime", processingTime);

        int iterCount = -1;
        getAttribute(*pRenders, "iterCount", iterCount);

        float minRMSE = -1.f;
        getAttribute(*pRenders, "minRMSE", minRMSE);

        uint32_t renderCount = 0;
        return runCompareRenderGroup(
            resultPath,
            *pRenders,
            nullptr,
            scene,
            camera,
            iterCount,
            processingTime,
            minRMSE,
            renderCount,
            *referenceImage,
            callback,
            handleGlobalPreprocessParameters);
    }
    return true;
}

}
