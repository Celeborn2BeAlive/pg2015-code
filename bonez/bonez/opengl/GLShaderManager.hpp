#pragma once

#include <bonez/common.hpp>
#include <unordered_map>
#include <bonez/sys/files.hpp>

#include "utils/GLShader.hpp"
#include "utils/GLProgram.hpp"

namespace BnZ {

class GLShaderManager {
public:
    GLShaderManager();

    GLShaderManager(const FilePath& dirPath);

    void addDirectory(const FilePath& dirPath);

    const GLShader& getShader(const std::string& name) const;

    GLProgram buildProgram(const std::vector<std::string>& shaderFilePaths) const;

private:
    void recursiveCompileShaders(const FilePath& relativePath, const Directory& dir);

    std::unordered_map<std::string, GLenum> m_ExtToType;
    std::unordered_map<std::string, GLShader> m_ShadersMap;

    el::Logger* m_pLogger = el::Loggers::getLogger("GLShaderManager");
};

}
