#include "threads.hpp"
#include <unordered_map>

namespace BnZ {

ParallelProcessor ParallelProcessor::s_Instance;

static std::unordered_map<std::thread::id, bool> s_ThreadFlagsMap;
static std::mutex s_TheadFlagsMapMutex;

bool isCurrentThreadDebugFlagEnabled() {
    std::unique_lock<std::mutex> l(s_TheadFlagsMapMutex);
    return s_ThreadFlagsMap[std::this_thread::get_id()];
}

void setCurrentThreadDebugFlag(bool value) {
    std::unique_lock<std::mutex> l(s_TheadFlagsMapMutex);
    s_ThreadFlagsMap[std::this_thread::get_id()] = value;
}

}
