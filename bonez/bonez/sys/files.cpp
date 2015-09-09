#include "files.hpp"

#include <fstream>

#ifdef _WIN32

#include <windows.h>
#include <windef.h>
#include <shlwapi.h>

#endif

namespace BnZ {

void copyFile(const FilePath& srcFilePath, const FilePath& dstFilePath) {
    std::ifstream src(srcFilePath.c_str(), std::ios::binary);
    std::ofstream dst(dstFilePath.c_str(), std::ios::binary);

    dst << src.rdbuf();
}

#ifndef __GNUC__

Directory::Directory(const FilePath& path) :
    m_Path(path),
    m_Handle(CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0)) {
}

Directory::~Directory() {
    if (*this) {
        CloseHandle(m_Handle);
    }
}

Directory::operator bool() const {
    return m_Handle != INVALID_HANDLE_VALUE;
}

const FilePath& Directory::path() const {
    return m_Path;
}

std::vector<FilePath> Directory::files() const {
    std::vector<FilePath> container;

    std::string path = m_Path.str() + "\\*";

    WIN32_FIND_DATA ffd;
    auto hFind = FindFirstFile(path.c_str(), &ffd);

    if (INVALID_HANDLE_VALUE == hFind) {
        return container;
    }

    do {
        FilePath file(ffd.cFileName);
        if (file != ".." && file != ".") {
            container.emplace_back(file);
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    return container;
}

bool exists(const FilePath& path) {
    return PathFileExists(path.c_str());
}

bool isDirectory(const FilePath& path) {
    return PathIsDirectory(path.c_str());
}

bool isRegularFile(const FilePath& path) {
    return exists(path) && !isDirectory(path);
}

void createDirectory(const FilePath& path) {
    CreateDirectory(path.c_str(), 0);
}

#endif

}
