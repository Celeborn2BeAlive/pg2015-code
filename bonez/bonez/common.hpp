#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <numeric>

#include "sys/time.hpp"
#include "sys/files.hpp"
#include "sys/easyloggingpp.hpp"

#include "utils/itertools/itertools.hpp"

// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

namespace BnZ {

using std::begin;
using std::end;

template<typename T>
inline std::string toString6(T&& value) {
    std::stringstream ss;
    ss << std::setfill('0');
    ss << std::setw(6) << value;
    return ss.str();
}

template<typename T>
inline std::string toString3(T&& value) {
    std::stringstream ss;
    ss << std::setfill('0');
    ss << std::setw(3) << value;
    return ss.str();
}

template<typename T>
inline std::string toString(T&& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

template<typename T>
inline T lexicalCast(const std::string& str) {
    std::stringstream ss;
    ss << str;
    T t;
    ss >> t;
    return t;
}

inline std::string getUniqueName() {
    return toString(getMicroseconds());
}

template<typename T>
std::vector<T> concat(std::vector<T> v1, const std::vector<T>& v2) {
    v1.insert(end(v1), begin(v2), end(v2));
    return v1;
}

inline bool stringEndsWith(const std::string& str,
                           const std::string& endStr) {
    if(str.size() < endStr.size()) {
        return false;
    }
    return endStr == str.substr(str.size() - endStr.size(),
                                endStr.size());
}

template<typename T>
static void storeArray(const FilePath& path, const std::vector<T>& values) {
    std::ofstream file(path.c_str());
    for(const auto& v: values) {
        file << v << " ";
    }
}

template<typename T>
static void loadArray(const FilePath& path, std::vector<T>& values) {
    values.clear();
    std::ifstream file(path.c_str());
    std::istream_iterator<T> eos;              // end-of-stream iterator
    std::istream_iterator<T> iit (file);
    std::copy(iit, eos, std::back_inserter(values));
}

class UpdateFlag {
public:
    uint64_t m_nCount = 0;

    bool operator ==(const UpdateFlag& flag) const {
        return m_nCount == flag.m_nCount;
    }

    bool operator !=(const UpdateFlag& flag) const {
        return !(*this == flag);
    }

    void addUpdate() {
        ++m_nCount;
    }

    bool hasChangedAndUpdate(UpdateFlag& flag) const {
        if(flag != *this) {
            flag = *this;
            return true;
        }
        return false;
    }
};

}

#if __GNUC__
namespace std {
    template <typename T, size_t N>
    inline size_t size(T const (&)[N])
    {
        return N;
    }

    template <class C>
    inline size_t size(C const & c)
    {
        return c.size();
    }
}
#endif
