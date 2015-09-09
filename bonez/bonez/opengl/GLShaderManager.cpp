#include "GLShaderManager.hpp"
#include "bonez/sys/files.hpp"

#include <stdexcept>

namespace BnZ {

GLShaderManager::GLShaderManager() {
    m_ExtToType["vs"] = GL_VERTEX_SHADER;
    m_ExtToType["gs"] = GL_GEOMETRY_SHADER;
    m_ExtToType["fs"] = GL_FRAGMENT_SHADER;
    m_ExtToType["cs"] = GL_COMPUTE_SHADER;
}

GLShaderManager::GLShaderManager(const FilePath& dirPath):
    GLShaderManager() {
    addDirectory(dirPath);
}

void GLShaderManager::addDirectory(const FilePath& dirPath) {
    m_pLogger->info("Compile shaders from directory %v...", dirPath);

    Directory dir(dirPath);

    if(!dir) {
        throw std::runtime_error("Unable to open shader directory " + dirPath.str());
    }

    recursiveCompileShaders("", dir);
}

void GLShaderManager::recursiveCompileShaders(const FilePath& relativePath,
                                              const Directory& dir) {
    for(const auto& file: dir.files()) {
        auto completePath = dir.path() + file;
        if(isRegularFile(completePath)) {
            if(file.ext() == "xs") {
                auto src = loadShaderSource(completePath.str());
                auto baseName = (relativePath + file).str();
                baseName = baseName.substr(0, baseName.size() - 2);

                for(const auto& pair: m_ExtToType) {
                    auto name = baseName + pair.first;
                    m_pLogger->verbose(1, "Compile shader %v...", name);
                    try {
                        m_ShadersMap.emplace(name, compileShader(pair.second, src));
                    } catch(...) {
                        m_pLogger->error("Error compiling shader %v", completePath);
                        throw;
                    }
                }
            } else {
                auto it = m_ExtToType.find(file.ext());
                if(it != std::end(m_ExtToType)) {
                    m_pLogger->verbose(1, "Compile shader %v...", relativePath + file);
                    try {
                        m_ShadersMap.emplace((relativePath + file).str(), compileShader((*it).second, loadShaderSource(completePath.str())));
                    } catch(...) {
                        m_pLogger->error("Error compiling shader %v", completePath);
                        throw;
                    }
                }
            }
        } else if(isDirectory(completePath)) {
            recursiveCompileShaders(relativePath + file, Directory(completePath));
        }
    }
}

const GLShader& GLShaderManager::getShader(const std::string& name) const {
    FilePath path(name);
    auto it = m_ShadersMap.find(path.str());
    if(it == std::end(m_ShadersMap)) {
        m_pLogger->error("Shader %v not found.", name);
        throw std::runtime_error("Shader " + name + " not found");
    }
    return (*it).second;
}

GLProgram GLShaderManager::buildProgram(const std::vector<std::string>& shaders) const {
    GLProgram program;
    for (const auto& shader : shaders) {
        program.attachShader(getShader(shader));
    }

    if (!program.link()) {
        std::cerr << program.getInfoLog() << std::endl;
        throw std::runtime_error(program.getInfoLog());
    }

    return program;
}

}
