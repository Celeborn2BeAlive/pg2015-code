#pragma once

#include <cstdint>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>

#include <bonez/utils/MultiDimensionalArray.hpp>
#include <bonez/utils/itertools/Range.hpp>

namespace BnZ {

using std::chrono::duration_cast;

using Nanoseconds = std::chrono::nanoseconds;
using Microseconds = std::chrono::microseconds;

inline double ns2sec(uint64_t t) {
    return t * 0.000000001;
}

inline double ns2ms(uint64_t t) {
    return t * 0.000001;
}

inline double us2ms(uint64_t t) {
    return t * 0.001;
}

inline double us2ms(Microseconds t) {
    return us2ms(t.count());
}

inline double us2sec(uint64_t t) {
    return t * 0.000001;
}

inline double us2sec(Microseconds t) {
    return us2sec(t.count());
}

inline double ms2sec(double t) {
    return t * 0.001;
}

std::string getDateString();

uint64_t getMicroseconds();

class Timer {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    bool m_bDisplayTimeAtEnd;
    TimePoint m_StartPoint;

public:
    Timer(bool displayTimeAtEnd = false) :
        m_bDisplayTimeAtEnd(displayTimeAtEnd),
        m_StartPoint(Clock::now()) {
    }

    ~Timer() {
        if (m_bDisplayTimeAtEnd) {
            std::clog << "Ellapsed time = " << us2ms(getMicroEllapsedTime()) << " ms." << std::endl;
        }
    }

    Microseconds getMicroEllapsedTime() const {
        return std::chrono::duration_cast<Microseconds>(Clock::now() - m_StartPoint);
    }

    Nanoseconds getNanoEllapsedTime() const {
        return std::chrono::duration_cast<Nanoseconds>(Clock::now() - m_StartPoint);
    }

    template<typename DurationType>
    DurationType getEllapsedTime() const {
        return std::chrono::duration_cast<DurationType>(Clock::now() - m_StartPoint);
    }
};

template<typename Iterator>
inline const typename std::iterator_traits<Iterator>::value_type accumulateTime(Iterator&& begin, Iterator&& end) {
    using DurationType = typename std::iterator_traits<Iterator>::value_type;
    return std::accumulate(
                std::forward<Iterator>(begin),
                std::forward<Iterator>(end),
                DurationType{ 0 },
                std::plus<DurationType>());
}

template<typename DurationType>
inline const DurationType accumulateTime(const std::vector<DurationType>& times) {
    return accumulateTime(begin(times), end(times));
}

template<typename DurationType>
struct EvalCallTimeRAII {
    Timer m_Timer;
    DurationType& m_TotalTime;

    EvalCallTimeRAII(DurationType& totalTime): m_TotalTime(totalTime) {
    }

    ~EvalCallTimeRAII() {
        m_TotalTime += m_Timer.getEllapsedTime<DurationType>();
    }
};

template<typename DurationType, typename Functor, typename... Args>
inline auto evalCallTime(DurationType& totalTime,
             Functor&& f,
             Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
    EvalCallTimeRAII<DurationType> timer(totalTime);
    return f(std::forward<Args>(args)...);
}

class TaskTimer {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    std::vector<std::string> m_TaskNames;
    MultiDimensionalArray<Duration, 2> m_TaskDurations; // Duration for each task and for each thread

public:
    class TimerRAII {
        std::size_t m_nThreadID;
        std::size_t m_nTaskID;
        TaskTimer& m_Timer;
        TimePoint m_StartPoint;
        bool m_bHasStoredDuration = false;
    public:
        TimerRAII(std::size_t taskID, std::size_t threadID, TaskTimer& timer):
            m_nThreadID(threadID),
            m_nTaskID(taskID),
            m_Timer(timer),
            m_StartPoint(Clock::now()) {
        }

        void storeDuration() {
            m_Timer.m_TaskDurations(m_nTaskID, m_nThreadID) += (Clock::now() - m_StartPoint);
            m_bHasStoredDuration = true;
        }

        ~TimerRAII() {
            if(!m_bHasStoredDuration) {
                storeDuration();
            }
        }
    };
    friend class TimerRAII;

    TaskTimer() = default;

    TaskTimer(std::vector<std::string> taskNames, std::size_t threadCount):
        m_TaskNames(std::move(taskNames)),
        m_TaskDurations(m_TaskNames.size(), threadCount) {
        std::fill(begin(m_TaskDurations), end(m_TaskDurations), Duration(0));
    }

    TimerRAII start(std::size_t taskID, std::size_t threadID = 0) {
        return { taskID, threadID, *this };
    }

    std::size_t getTaskCount() const {
        return m_TaskNames.size();
    }

    const std::string& getTaskName(std::size_t taskID) const {
        return m_TaskNames[taskID];
    }

    template<typename DurationType>
    DurationType getEllapsedTime(std::size_t taskID) const {
        Duration taskDuration(0);
        for(auto threadID: range(m_TaskDurations.size(1))) {
            taskDuration += m_TaskDurations(taskID, threadID);
        }
        return std::chrono::duration_cast<DurationType>(taskDuration);
    }
};

template<typename DurationType>
inline std::vector<DurationType> computeTaskDurations(const TaskTimer& timer) {
    std::vector<DurationType> taskDurations;
    taskDurations.reserve(timer.getTaskCount());
    for(auto taskID: range(timer.getTaskCount())) {
        taskDurations.emplace_back(timer.getEllapsedTime<DurationType>(taskID));
    }
    return taskDurations;
}

}
