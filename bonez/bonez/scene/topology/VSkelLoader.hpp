#pragma once

#include "bonez/sys/files.hpp"
#include "CurvilinearSkeleton.hpp"

namespace BnZ {

class VSkelLoader {
public:
    VSkelLoader(const FilePath& basePath = ""):
        m_BasePath(basePath) {
    }

    CurvilinearSkeleton loadCurvilinearSkeleton(const FilePath& filepath);
private:
    FilePath m_BasePath;
};

}
