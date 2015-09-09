#include "time.hpp"
#include <chrono>

namespace BnZ {

uint64_t getMicroseconds() {
    return std::chrono::duration_cast<Microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

std::string getDateString() {
    time_t t = time(nullptr);
    char mbstr[1024];
    strftime(mbstr, sizeof(mbstr), "%Y-%b-%d-%a-%H-%M-%S", localtime(&t));
    return std::string(mbstr);
}

}
