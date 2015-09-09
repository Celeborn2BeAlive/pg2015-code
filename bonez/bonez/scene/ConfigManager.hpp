#pragma once

#include "bonez/common.hpp"
#include "bonez/sys/files.hpp"
#include "bonez/parsing/parsing.hpp"

#include "sensors/ProjectiveCamera.hpp"
#include "lights/LightContainer.hpp"

namespace BnZ {

class ConfigManager {
public:
    ConfigManager() = default;

    void setPath(FilePath path);

    const FilePath& getPath() const {
        return m_Path;
    }

    std::size_t storeConfig(const std::string& name, const ProjectiveCamera& camera,
                     const LightContainer& lights);

    FilePath loadConfig(const std::string& name, ProjectiveCamera& camera,
                        Scene& scene, float resX, float resY, float zNear, float zFar) const throw(std::runtime_error);

    FilePath loadConfig(uint32_t index, ProjectiveCamera& camera,
                        Scene& scene, float resX, float resY, float zNear, float zFar) const throw(std::runtime_error);

    FilePath loadConfig(const std::string& name, ProjectiveCamera& camera,
                        Scene& scene, float resX, float resY, float zNear, float zFar,
                        tinyxml2::XMLDocument& document) const throw(std::runtime_error);

    FilePath loadConfig(uint32_t index, ProjectiveCamera& camera,
                        Scene& scene, float resX, float resY, float zNear, float zFar,
                        tinyxml2::XMLDocument& document) const throw(std::runtime_error);

    std::string getConfigName(uint32_t index) const throw(std::out_of_range) {
        if(index >= m_Configs.size()) {
            throw std::out_of_range("Configuration index out of range");
        }
        return m_Configs[index];
    }

    int getConfigIndex(const std::string& name) const {
        auto it = std::find(begin(m_Configs), end(m_Configs), name);
        if(it == end(m_Configs)) {
            return -1;
        }
        return it - begin(m_Configs);
    }

    const std::vector<std::string>& getConfigs() const {
        return m_Configs;
    }

private:
    FilePath m_Path;
    std::vector<std::string> m_Configs;
};

}
