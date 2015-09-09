#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <bonez/types.hpp>

namespace BnZ {

class ParallelProcessor {
    uint32_t m_nThreadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> m_Threads;
    std::mutex m_DebugMutex;

    ParallelProcessor() {
        m_Threads.reserve(m_nThreadCount);
    }

public:
    static ParallelProcessor s_Instance;

    uint32_t getSystemThreadCount() const {
        return m_nThreadCount;
    }

    template<typename TaskFunctor>
    void launchThreads(const TaskFunctor& task, uint32_t threadCount) {
        m_Threads.clear();
        for(auto i = 0u; i < threadCount; ++i) {
            m_Threads.emplace_back(std::thread(task, i));
        }

        for(auto i = 0u; i < threadCount; ++i) {
            m_Threads[i].join();
        }
    }

    // Lock the debug mutex, use this function in a scope
    friend std::unique_lock<std::mutex> debugLock();
};

inline uint32_t getSystemThreadCount() {
    return ParallelProcessor::s_Instance.getSystemThreadCount();
}

template<typename TaskFunctor>
inline void launchThreads(const TaskFunctor& task,
                          uint32_t threadCount) {
    ParallelProcessor::s_Instance.launchThreads(task, threadCount);
}

template<typename TaskFunctor>
inline void processTasks(uint32_t taskCount,
                         const TaskFunctor& task,
                         uint32_t threadCount) {
    auto blockSize = std::max(1u, taskCount / threadCount);

    std::atomic_uint nextBatch;
    nextBatch.store(0);

    auto batchProcess = [&](uint32_t threadID) {
        while(true) {
            auto batchID = nextBatch++;
            auto taskID = batchID * blockSize;

            if(taskID >= taskCount) {
                break;
            }

            auto end = std::min(taskID + blockSize, taskCount);

            while (taskID < end) {
                task(taskID, threadID);
                ++taskID;
            }
        }
    };

    launchThreads(batchProcess, threadCount);
}

template<typename TaskFunctor>
inline void processTasksDeterminist(uint32_t taskCount,
                         const TaskFunctor& task,
                         uint32_t threadCount) {
    auto blockSize = std::max(1u, taskCount / threadCount);

    auto batchProcess = [&](uint32_t threadID) {
        auto loopID = 0u;
        while(true) {
            auto batchID = loopID * threadCount + threadID;
            ++loopID;

            auto taskID = batchID * blockSize;

            if(taskID >= taskCount) {
                break;
            }

            auto end = std::min(taskID + blockSize, taskCount);

            for (; taskID < end; ++taskID) {
                task(taskID, threadID);
            }
        }
    };

    launchThreads(batchProcess, threadCount);
}

inline std::unique_lock<std::mutex> debugLock() {
    return std::unique_lock<std::mutex>(ParallelProcessor::s_Instance.m_DebugMutex);
}

bool isCurrentThreadDebugFlagEnabled();

void setCurrentThreadDebugFlag(bool value);

struct CurrentThreadDebugFlagRAII {
    CurrentThreadDebugFlagRAII(bool condition) {
        setCurrentThreadDebugFlag(condition);
    }
    ~CurrentThreadDebugFlagRAII() {
        setCurrentThreadDebugFlag(false);
    }
    CurrentThreadDebugFlagRAII(const CurrentThreadDebugFlagRAII&) = delete;
    CurrentThreadDebugFlagRAII& operator =(const CurrentThreadDebugFlagRAII&) = delete;
};

// A Simple class to manipulate values computed over multiple
// threads
template<typename T, size_t MaxThreadCount>
class ThreadsValue {
    T m_Values[MaxThreadCount];
public:
    void init(const T& value) {
        std::fill(std::begin(m_Values), std::end(m_Values), value);
    }

    const T& operator [](uint32_t threadIndex) const {
        return m_Values[threadIndex];
    }

    T& operator [](uint32_t threadIndex) {
        return m_Values[threadIndex];
    }

    T sum() const {
        T result = m_Values[0];
        for (auto i = 1u; i < MaxThreadCount; ++i) {
            result += m_Values[i];
        }
        return result;
    }
};

}
