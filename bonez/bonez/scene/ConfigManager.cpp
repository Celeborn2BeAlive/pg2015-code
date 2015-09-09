#include "ConfigManager.hpp"

namespace BnZ {

static const std::string CONFIG_FILE_EXTENSION = ".config.bnz.xml";

void ConfigManager::setPath(FilePath path) {
    m_Path = std::move(path);
    createDirectory(m_Path);
    Directory dir(m_Path);
    if(!dir) {
        throw std::runtime_error("Unable to open configurations directory");
    }
    for(const auto& config: dir.files()) {
        if(stringEndsWith(config.str(), CONFIG_FILE_EXTENSION)) {
            m_Configs.emplace_back(config.str().substr(0, config.str().size() - CONFIG_FILE_EXTENSION.size()));
        }
    }
}

std::size_t ConfigManager::storeConfig(const std::string& name_,
                                const ProjectiveCamera& camera,
                                const LightContainer& lights) {
    auto name = name_;
    auto path = m_Path + (name + CONFIG_FILE_EXTENSION);
    if(exists(path)) {
        name = name_ + "-" + getUniqueName();
        path = m_Path + (name + CONFIG_FILE_EXTENSION);
    }
    tinyxml2::XMLDocument doc;
    auto pRoot = doc.NewElement("Config");
    doc.InsertFirstChild(pRoot);

    auto pCamera = doc.NewElement("ProjectiveCamera");
    pRoot->InsertEndChild(pCamera);

    storePerspectiveCamera(*pCamera, camera);

    auto pLights = doc.NewElement("Lights");
    pRoot->InsertEndChild(pLights);

    storeLights(lights, *pLights);

    doc.SaveFile(path.c_str());

    m_Configs.emplace_back(name);
    return m_Configs.size() - 1;
}

FilePath ConfigManager::loadConfig(const std::string& name, ProjectiveCamera& camera,
                Scene& scene, float resX, float resY, float zNear, float zFar) const throw(std::runtime_error) {
    tinyxml2::XMLDocument doc;
    return loadConfig(name, camera, scene, resX, resY, zNear, zFar, doc);
}

FilePath ConfigManager::loadConfig(uint32_t index, ProjectiveCamera& camera,
                Scene& scene, float resX, float resY, float zNear, float zFar) const throw(std::runtime_error) {
    tinyxml2::XMLDocument doc;
    return loadConfig(index, camera, scene, resX, resY, zNear, zFar, doc);
}

FilePath ConfigManager::loadConfig(const std::string& name,
                               ProjectiveCamera& camera,
                               Scene& scene,
                               float resX, float resY, float zNear, float zFar,
                               tinyxml2::XMLDocument& document) const throw(std::runtime_error) {
    auto path = m_Path + (name + CONFIG_FILE_EXTENSION);
    auto pDoc = &document;

    if(tinyxml2::XML_NO_ERROR != pDoc->LoadFile(
                path.c_str())) {
        throw std::runtime_error("ConfigManager: Unable to load config " + name);
    }
    auto pRoot = pDoc->RootElement();
    auto pCamera = pRoot->FirstChildElement("ProjectiveCamera");

    Vec3f eye, point, up;
    float fovY;

    loadPerspectiveCamera(*pCamera, eye, point, up, fovY);

    camera = ProjectiveCamera(lookAt(eye, point, up), fovY, resX, resY, zNear, zFar);

    scene.clearLights();
    loadLights(scene, pRoot->FirstChildElement("Lights"));

    return path;
}

FilePath ConfigManager::loadConfig(uint32_t index, ProjectiveCamera& camera,
                Scene& scene, float resX, float resY, float zNear, float zFar,
                tinyxml2::XMLDocument& document) const throw(std::runtime_error) {
    if(index >= m_Configs.size()) {
        std::cerr << "Config index out of range" << std::endl;
        return {};
    }
    return loadConfig(m_Configs[index], camera, scene, resX, resY, zNear, zFar);
}

}
