#include "DebugLog.hpp"
#include <chrono>

namespace BnZ {

static std::ofstream g_DebugLog("bonez.log", std::ios_base::app);

void startDebugLog(const char* file, int line) {
   time_t t = time(nullptr);
   char mbstr[1024];
   strftime(mbstr, sizeof(mbstr), "%a %b %d %H:%M:%S %Y", localtime(&t));
   g_DebugLog << std::endl << mbstr << " " << file << " (" << line << "): " << std::endl;
}

std::ostream& debugLog() {
    g_DebugLog << "    ";
    return g_DebugLog;
}

}
