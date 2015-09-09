#pragma once

#ifdef __GNUC__

// Use Posix API
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#endif

#include <string>
#include <ostream>
#include <iostream>
#include <vector>

#ifdef _WIN32

typedef void* HANDLE;

#endif

namespace BnZ {

class FilePath {
public:
#ifdef _WIN32
    static const char PATH_SEPARATOR = '\\';
#else
    static const char PATH_SEPARATOR = '/';
#endif

    FilePath() = default;

    FilePath(const char* filepath): m_FilePath(filepath) {
        format();
    }

    FilePath(const std::string& filepath): m_FilePath(filepath) {
        format();
    }

    operator std::string() const { return m_FilePath; }

    const std::string& str() const { return m_FilePath; }

    const char* c_str() const { return m_FilePath.c_str(); }

    bool empty() const {
        return m_FilePath.empty();
    }

    /*! returns the containing directory path of a file */
    FilePath directory() const {
        size_t pos = m_FilePath.find_last_of(PATH_SEPARATOR);
        if (pos == std::string::npos) { return FilePath(); }
            return m_FilePath.substr(0, pos);
        }

    /*! returns the file of a filepath  */
    std::string file() const {
        size_t pos = m_FilePath.find_last_of(PATH_SEPARATOR);
        if (pos == std::string::npos) { return m_FilePath; }
        return m_FilePath.substr(pos + 1);
    }

    /*! returns the file extension */
    std::string ext() const {
        size_t pos = m_FilePath.find_last_of('.');
        if (pos == std::string::npos || pos == 0) { return ""; }
        return m_FilePath.substr(pos + 1);
    }

    bool hasExt(const std::string& ext) const {
        int offset = (int) m_FilePath.size() - (int) ext.size();
        return offset >= 0 && m_FilePath.substr(offset, ext.size()) == ext;
    }

    /*! adds file extension */
    FilePath addExt(const std::string& ext = "") const {
        return FilePath(m_FilePath + ext);
    }

    /*! concatenates two filepaths to this/other */
    FilePath operator +(const FilePath& other) const {
        if (m_FilePath.empty()) {
            return other;
        } else {
            if(other.empty()) {
                return m_FilePath;
            }
            FilePath copy(*this);
            if(other.m_FilePath.front() != PATH_SEPARATOR) {
                copy.m_FilePath += PATH_SEPARATOR;
            }
            copy.m_FilePath += other.m_FilePath;
            return copy;
        }
    }

    bool operator ==(const FilePath& other) const {
        return other.m_FilePath == m_FilePath;
    }

    bool operator !=(const FilePath& other) const {
        return !operator ==(other);
    }

    /*! output operator */
    friend std::ostream& operator<<(std::ostream& cout, const FilePath& filepath) {
        return (cout << filepath.m_FilePath);
    }

    bool isAbsolute() const {
        return !m_FilePath.empty() && m_FilePath[0] == '/';
    }

private:
    void format() {
        for (size_t i = 0; i < m_FilePath.size(); ++i) {
            if (m_FilePath[i] == '\\' || m_FilePath[i] == '/') {
                m_FilePath[i] = PATH_SEPARATOR;
            }
        }
        while (!m_FilePath.empty() && m_FilePath.back() == PATH_SEPARATOR) {
            m_FilePath.pop_back();
        }
    }

    std::string m_FilePath;
};

void copyFile(const FilePath& srcFilePath, const FilePath& dstFilePath);

#ifdef __GNUC__

class Directory {
public:
    Directory(const FilePath& path):
        m_Path(path), m_pDir(opendir(m_Path.c_str())) {
    }

    ~Directory() {
        if(m_pDir) {
            closedir(m_pDir);
        }
    }

    operator bool() const {
        return m_pDir;
    }

    const FilePath& path() const {
        return m_Path;
    }

    std::vector<FilePath> files() const {
        std::vector<FilePath> container;

        rewinddir(m_pDir);
        struct dirent* entry = nullptr;
        while(nullptr != (entry = readdir(m_pDir))) {
            FilePath file(entry->d_name);
            if(file != ".." && file != ".") {
                container.emplace_back(file);
            }
        }
        rewinddir(m_pDir);

        return container;
    }

private:
    FilePath m_Path;
    DIR* m_pDir;
};

inline bool exists(const FilePath& path) {
    struct stat s;
    return 0 == stat(path.c_str(), &s);
}

inline bool isDirectory(const FilePath& path) {
    struct stat s;
    return 0 == stat(path.c_str(), &s) && S_ISDIR(s.st_mode);
}

inline bool isRegularFile(const FilePath& path) {
    struct stat s;
    return 0 == stat(path.c_str(), &s) && S_ISREG(s.st_mode);
}

inline void createDirectory(const FilePath& path) {
#ifdef MINGW
     mkdir(path.c_str());
#else
    mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
#endif
}

#else

class Directory {
public:
    Directory(const FilePath& path);

    ~Directory();

    operator bool() const;

    const FilePath& path() const;

    std::vector<FilePath> files() const;

private:
    FilePath m_Path;
    HANDLE m_Handle;
};

bool exists(const FilePath& path);

bool isDirectory(const FilePath& path);

bool isRegularFile(const FilePath& path);

void createDirectory(const FilePath& path);


#endif

}

//namespace std {
//    template<> struct hash<BnZ::FilePath> {
//        size_t operator ()(const BnZ::FilePath& filepath) const {
//            std::hash<std::string> hash_fn;
//            return hash_fn(filepath.str());
//        }
//    };
//}
