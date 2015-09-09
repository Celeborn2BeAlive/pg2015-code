#pragma once

#include <bonez/common.hpp>
#include <bonez/rendering/renderers/recursive_mis_bdpt.hpp>
#include <bonez/rendering/renderers/DirectImportanceSampleTilePartionning.hpp>

#include "PG15BPTRenderer.hpp"
#include "PG15SkelBPTRenderer.hpp"
#include "PG15ICBPTRenderer.hpp"

namespace BnZ {

void storeFramebuffer(int resultIndex,
                      const FilePath& pngDir,
                      const FilePath& exrDir,
                      const FilePath& baseName,
                      float gamma,
                      const Framebuffer& framebuffer);

struct RenderStatistics {
    Microseconds renderTime { 0 }; // Current render time

    // Render time at each frame
    std::vector<float> renderTimes;

    // Errors at each frame
    std::vector<float> nrmse;
    std::vector<float> rmse;
    std::vector<float> mae;

    std::size_t getIterationCount() const {
        return renderTimes.size();
    }
};

// This class is in charge of rendering and comparing the three algorithms: BPT, ICBPT and SkelBPT
// For SkelBPT, multiple instances can be run to try for various parameters
// All results are stored in a repertory:
// - "Date_(renderTime | iterCount)"
//      - exr: contains EXR images
//      - png: contains PNG images (000 is BPT, 001 is ICBPT and 002-xxx are SkelBPT)
//      - reports: contains XML reports containing informations about the rendering
//      - stats: contains arrays of error values to plot error curves
//      - thinning.report.bnz.xml: contains informations about the thinning time
//      - scene.bnz.xml: contains the scene description used for the rendering
//      - config.bnz.xml: contains the scene configuration used for the rendering
//
// See render() method for more informations
class PG15RendererManager {
public:
    PG15RendererManager(const Scene& scene, const Sensor& sensor,
                        Vec2u framebufferSize, const FilePath& referenceImagePath,
                        const FilePath& resultPath,
                        tinyxml2::XMLDocument& configDoc,
                        tinyxml2::XMLDocument& sceneDoc,
                        float gamma,
                        std::size_t renderTimeMsOrIterationCount,
                        std::size_t maxPathDepth,
                        std::size_t resamplingPathCount,
                        const PG15ICBPTSettings& icBPTSettings,
                        const std::vector<PG15SkelBPTSettings>& skelBPTSettings,
                        bool equalTime):
        m_ResultPath(resultPath),
        m_Params(scene, sensor, framebufferSize, maxPathDepth, resamplingPathCount),
        m_SharedData(framebufferSize.x * framebufferSize.y, m_Params.m_nMaxDepth - 1u),
        m_pReferenceImage(loadEXRImage(referenceImagePath.str())),
        m_ICBPTRenderer(m_Params, m_SharedData, icBPTSettings),
        m_RenderStatistics(2 + skelBPTSettings.size()),
        m_fGamma(gamma),
        m_nRenderTimeMsOrIterationCount(renderTimeMsOrIterationCount),
        m_bEqualTime(equalTime) {

        for(const auto& settings: skelBPTSettings) {
            m_SkelBPTRenderers.emplace_back(m_Params, m_SharedData, settings);
        }

        m_Rng.init(getSystemThreadCount(), m_nSeed);

        m_SharedData.m_LightSampler.initFrame(scene);

        initResultDir(configDoc, sceneDoc);
    }

    const FilePath& getResultPath() const {
        return m_ResultPath;
    }

    void initResultDir(tinyxml2::XMLDocument& configDoc,
                        tinyxml2::XMLDocument& sceneDoc) {
        createDirectory(m_ResultPath);
        auto resultName = getDateString() + "_" + toString(m_nRenderTimeMsOrIterationCount);

        m_ResultPath = m_ResultPath + resultName;
        createDirectory(m_ResultPath);

        configDoc.SaveFile((m_ResultPath + "config.bnz.xml").c_str());
        sceneDoc.SaveFile((m_ResultPath + "scene.bnz.xml").c_str());
    }

    const Image& getReferenceImage() const {
        return *m_pReferenceImage;
    }

    const Framebuffer& getCurrentFramebuffer() const {
        if(m_nDisplayRendererIndex == 0u) {
            return m_BPTRenderer.getFramebuffer();
        } else if(m_nDisplayRendererIndex == 1u) {
            return m_ICBPTRenderer.getFramebuffer();
        }
        return m_SkelBPTRenderers[m_nDisplayRendererIndex - 2].getFramebuffer();
    }

    template<typename RendererType>
    bool render(RendererType& renderer, std::size_t index) {
        auto& stats = m_RenderStatistics[index];

        if(m_bEqualTime && us2ms(stats.renderTime) >= m_nRenderTimeMsOrIterationCount) {
            pLogger->info("Already done.");
            return true;
        }

        Timer timer;
        renderer.render();
        stats.renderTime += timer.getEllapsedTime<Microseconds>();

        auto& framebuffer = ((const RendererType&)renderer).getFramebuffer();

        auto nrmse = computeNormalizedRootMeanSquaredError(*m_pReferenceImage, framebuffer.getChannel(0));
        auto nrmseFloat = reduceMax(nrmse);

        auto rmse = computeRootMeanSquaredError(*m_pReferenceImage, framebuffer.getChannel(0));
        auto rmseFloat = reduceMax(rmse);

        auto mae = computeMeanAbsoluteError(*m_pReferenceImage, framebuffer.getChannel(0));
        auto maeFloat = reduceMax(mae);

        stats.renderTimes.emplace_back(us2ms(stats.renderTime));
        stats.nrmse.emplace_back(nrmseFloat);
        stats.rmse.emplace_back(rmseFloat);
        stats.mae.emplace_back(maeFloat);

        pLogger->info("Time = %v | NRMSE = %v | RMSE = %v | MAE = %v",
                      us2ms(stats.renderTime), nrmseFloat, rmseFloat, maeFloat);

        return false;
    }

    // Render in a loop all rendering algorithms, one iteration at a time until each algorithm has finished
    // All light paths are shared by all algorithms for a given iteration
    bool render() {
        bool allDone = true;

        pLogger->info("Start iteration %v", m_SharedData.m_nIterationCount);

        // Sample light paths and setup data to project light vertices on image plane
        auto initFrameTime = [&](){
            Timer timer;

            {
                auto taskTimer = m_InitIterationTimer.start(0);
                sampleLightPaths();
            }

            {
                auto taskTimer = m_InitIterationTimer.start(1);
                buildDirectImportanceSampleTilePartitionning();
            }

            return timer.getEllapsedTime<Microseconds>();
        }();

        for(auto& stats: m_RenderStatistics) {
            stats.renderTime += initFrameTime;
        }

        // Run all algorithms
        pLogger->info("Render BPT");
        allDone = render(m_BPTRenderer, 0) && allDone;
        pLogger->info("Render ICBPT");
        allDone = render(m_ICBPTRenderer, 1) && allDone;

        for(auto i: range(m_SkelBPTRenderers.size())) {
            pLogger->info("Render SkelBPT %v", i);
            allDone = render(m_SkelBPTRenderers[i], 2 + i) && allDone;
        }

        ++m_SharedData.m_nIterationCount;

        if(!m_bEqualTime && m_SharedData.m_nIterationCount == m_nRenderTimeMsOrIterationCount) {
            allDone = true;
        }

        if(allDone) {
            storeResults();
        }

        return allDone;
    }

    void drawGUI(GUI& gui) {
        if(auto window = gui.addWindow("RendererManager")) {
            std::vector<std::string> rendererNames = { "BPT", "ICBPT"};
            for(auto index: range(m_SkelBPTRenderers.size())) {
                rendererNames.emplace_back(std::string("SkelBPT ") + toString(index));
            }
            gui.addCombo("Display Framebuffer", m_nDisplayRendererIndex, rendererNames.size(), [&](auto index) {
                return rendererNames[index].c_str();
            });

            gui.addValue("Iteration", m_SharedData.m_nIterationCount);

            for(auto index: range(m_SkelBPTRenderers.size() + 2)) {
                gui.addSeparator();

                gui.addText(rendererNames[index]);

                auto& stats = m_RenderStatistics[index];
                gui.addValue("Time Ellapsed (ms)", us2ms(stats.renderTime));
                if(m_bEqualTime) {
                    gui.addValue("Remaining time", max(0.0, double(m_nRenderTimeMsOrIterationCount) - us2ms(stats.renderTime)));
                }
                gui.addValue("Mean Time Per Frame", us2ms(stats.renderTime) / m_SharedData.m_nIterationCount);

                if(!stats.nrmse.empty()) {
                    gui.addValue("NRMSE", stats.nrmse.back());
                }
                if(!stats.rmse.empty()) {
                    gui.addValue("RMSE", stats.rmse.back());
                }
                if(!stats.mae.empty()) {
                    gui.addValue("MAE", stats.mae.back());
                }
            }
        }
    }

    template<typename RendererType>
    void storeResults(const RendererType& renderer, std::size_t index) {
        FilePath baseName(toString3(index));

        FilePath pngDir = m_ResultPath + "png";
        FilePath exrDir = m_ResultPath + "exr";
        FilePath statsDir = m_ResultPath + "stats";

        storeFramebuffer(index, pngDir, exrDir, baseName, m_fGamma, renderer.getFramebuffer());

        createDirectory(statsDir);

        auto& stats = m_RenderStatistics[index];

        storeArray(statsDir + baseName.addExt(".nrmseFloat"), stats.nrmse);
        storeArray(statsDir + baseName.addExt(".rmseFloat"), stats.rmse);
        storeArray(statsDir + baseName.addExt(".absErrorFloat"), stats.mae);
        storeArray(statsDir + baseName.addExt(".processingTimes"), stats.renderTimes);

        auto reportsPath = m_ResultPath + "reports";
        createDirectory(reportsPath);

        auto reportDocumentPath = reportsPath + (toString3(index) + ".report.bnz.xml");
        tinyxml2::XMLDocument reportDocument;

        auto pReport = reportDocument.NewElement("Result");
        auto pRendererStats = reportDocument.NewElement("RendererStats");
        auto pRendererSettings = reportDocument.NewElement("Renderer");

        renderer.storeSettings(*pRendererSettings);
        renderer.storeStatistics(*pRendererStats);

        reportDocument.InsertEndChild(pReport);

        setAttribute(*pReport, "Index", index);
        setChildAttribute(*pReport, "Gamma", m_fGamma);

        setChildAttribute(*pReport, "RenderTime", stats.renderTimes.back());
        setChildAttribute(*pReport, "NRMSE", stats.nrmse.back());
        setChildAttribute(*pReport, "RMSE", stats.rmse.back());
        setChildAttribute(*pReport, "MAE", stats.mae.back());
        setChildAttribute(*pReport, "IterationCount", stats.getIterationCount());

        pReport->InsertEndChild(pRendererStats);

        pReport->InsertEndChild(pRendererSettings);

        reportDocument.SaveFile(reportDocumentPath.c_str());
    }

    void storeResults() {
        pLogger->info("Store BPT results");
        storeResults(m_BPTRenderer, 0);
        pLogger->info("Store ICBPT results");
        storeResults(m_ICBPTRenderer, 1);
        for(auto i: range(m_SkelBPTRenderers.size())) {
            pLogger->info("Store SkelBPT %v results", i);
            storeResults(m_SkelBPTRenderers[i], 2 + i);
        }
    }

    void sampleLightPaths() {
        auto mis = [&](float v) {
            return Mis(v);
        };

        processTasksDeterminist(m_SharedData.m_nLightPathCount, [&](uint32_t pathID, uint32_t threadID) {
            ThreadRNG rng(m_Rng, threadID);
            auto pLightPath = m_SharedData.m_LightVertexBuffer.getSlicePtr(pathID);
            m_SharedData.m_EmissionVertexBuffer[pathID] =
                    sampleLightPath(pLightPath, m_SharedData.m_nLightPathMaxDepth, m_Params.m_Scene,
                            m_SharedData.m_LightSampler, m_Params.m_nResamplingPathCount, mis, rng);
        }, getSystemThreadCount());
    }

    void buildDirectImportanceSampleTilePartitionning() {
        ThreadRNG rng(m_Rng, 0);
        m_SharedData.m_DirectImportanceSampleTilePartionning.build(
                    m_SharedData.m_LightVertexBuffer.size(),
                    m_Params.m_Scene,
                    m_Params.m_Sensor,
                    m_Params.m_FramebufferSize,
                    m_Params.m_TileSize,
                    m_Params.m_TileCount,
                    [&](std::size_t i) {
                        return m_SharedData.m_LightVertexBuffer[i].m_Intersection;
                    },
                    [&](std::size_t i) {
                        return m_SharedData.m_LightVertexBuffer[i].m_Intersection && m_SharedData.m_LightVertexBuffer[i].m_fPathPdf > 0.f;
                    },
                    rng);
    }

private:
    el::Logger* pLogger = el::Loggers::getLogger("PG15RendererManager");

    FilePath m_ResultPath;

    PG15RendererParams m_Params;
    PG15SharedData m_SharedData;
    Shared<Image> m_pReferenceImage;

    uint32_t m_nSeed = 1024u;
    mutable ThreadsRandomGenerator m_Rng;

    std::size_t m_nDisplayRendererIndex = 0;

    PG15BPTRenderer m_BPTRenderer { m_Params, m_SharedData };
    PG15ICBPTRenderer m_ICBPTRenderer;
    std::vector<PG15SkelBPTRenderer> m_SkelBPTRenderers;

    std::vector<RenderStatistics> m_RenderStatistics;

    float m_fGamma;
    std::size_t m_nRenderTimeMsOrIterationCount;
    bool m_bEqualTime = true;

    TaskTimer m_InitIterationTimer = {
        {
            "SampleLightPaths",
            "BuildTilePartionning",
        }, 1u
    };
};

inline void storeFramebuffer(int resultIndex,
                             const FilePath& pngDir,
                             const FilePath& exrDir,
                             const FilePath& baseName,
                             float gamma,
                             const Framebuffer& framebuffer) {
    // Create output directories in case they don't exist
    createDirectory(pngDir.str());
    createDirectory(exrDir.str());

    // Path of primary output image files
    FilePath pngFile = pngDir + baseName.addExt(".png");
    FilePath exrFile = exrDir + baseName.addExt(".exr");

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
    auto exrFramebufferFilePath = exrDir + baseName.addExt(".bnzframebuffer.exr");
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
        FilePath pngFile = pngFramebufferDirPath + FilePath(toString3(i)).addExt("_" + name + ".png");

        auto copy = framebuffer.getChannel(i);

        copy.flipY();
        copy.divideByAlpha();

        copy.applyGamma(gamma);
        storeImage(pngFile.str(), copy);
    }
}

}
