#include <iostream>

#include "PG2015Viewer.hpp"

INITIALIZE_EASYLOGGINGPP

namespace BnZ {

void generateResultsForPG15(const FilePath& applicationPath);

int main(int argc, char** argv) {
    initEasyLoggingpp(argc, argv);

    generateResultsForPG15(argv[0]);

	return 0;
}

struct SceneDescription {
    FilePath scenePath;
    std::string configName;
    FilePath referenceImagePath;
    FilePath resultPath;
};

void generateResultsForPG15(const FilePath& applicationPath) {
    // Scenes to render
    std::vector<SceneDescription> sceneDescriptions = {
        { "scenes/corridor/viewer.bnz.xml", // Parameters of the viewer and scene
          "far-from-light", // Name of the configuration to load
          "results/corridor/far-from-light/reference/reference.exr", // Path to the reference image
          "results/corridor/far-from-light/" // Path where to put the result folder
        },
        { "scenes/door/viewer.bnz.xml",
          "corner",
          "results/door/corner/reference/reference.exr",
          "results/door/corner/" },
        { "scenes/sponza/viewer.bnz.xml",
          "directional-light-from-power",
          "results/sponza/directional-light-from-power/reference/reference.exr",
          "results/sponza/directional-light-from-power"
        },
        { "scenes/sibenik/viewer.bnz.xml",
          "two-lights",
          "results/sibenik/two-lights/reference/reference.exr",
          "results/sibenik/two-lights/"
        }
    };

    // Run multiple instances for different comparison times (in milliseconds)
    std::vector<std::size_t> renderTimes = {
        /*10000, 30000,*/ 300000,/* 900000, 3600000, 7200000*/
    };

    PG15ICBPTSettings icBPTSettings; // Default settings for ICBPT
    std::vector<PG15SkelBPTSettings> skelBPTSettings; // Run multiple instances of SkelBPT, settings are stored in a vector
    // Try multiple filetering factors (see the article for more informations)
    for(auto filteringFactor: { /*-1.f, 0.1f, 0.5f, 0.9f, 1.5f*/ 1.f }) {
        skelBPTSettings.emplace_back(); // default settings
        skelBPTSettings.back().nodeFilteringFactor = filteringFactor; // Change filtering factor
    }

    // Compute lower bound on total render time
    std::size_t totalTime = 0;
    for(auto time: renderTimes) {
        totalTime += (2 + skelBPTSettings.size()) * time;
    }
    totalTime *= sceneDescriptions.size();
    LOG(INFO) << "Expected lower bound on render time = "
              << (ms2sec(totalTime) / 3600) << " hours" << std::endl;

    for(auto time: renderTimes) {
        for(const auto& scene: sceneDescriptions) {
            PG2015Viewer viewer(
                        applicationPath,
                        scene.scenePath,
                        scene.configName,
                        scene.referenceImagePath,
                        scene.resultPath,
                        time, // render time
                        6, // max path depth
                        1024, // resampling path count
                        icBPTSettings,
                        skelBPTSettings,
                        128, // thinning resolution (to compute the skeleton)
                        true, // use or not segmented skel
                        true); // equalTime (true) or equalIterationCount (false)
            viewer.run();
        }
    }
}

}

int main(int argc, char** argv) {
#ifdef _DEBUG
        std::cerr << "DEBUG MODE ACTIVATED" << std::endl;
#endif

#ifdef DEBUG
        std::cerr << "DEBUG MODE ACTIVATED" << std::endl;
#endif
    return BnZ::main(argc, argv);
}
