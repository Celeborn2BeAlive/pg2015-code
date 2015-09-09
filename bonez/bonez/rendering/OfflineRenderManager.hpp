#pragma once

namespace BnZ {

class OfflineRenderManager {
public:
    OfflineRenderManager(RendererManager& rendererManager);

    void renderConfig(const FilePath& basePath, const std::string& configName,
                      const ConfigManager& configManager,
                      Scene& scene, float zNear, float zFar);
private:
    Shared<Image> m_pReferenceImage;
};

}
