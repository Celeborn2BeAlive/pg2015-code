#pragma once

//#define ELPP_NO_DEFAULT_LOG_FILE
#define ELPP_DEBUG_ERRORS
#include <easyloggingpp/easylogging++.h>

#include <bonez/sys/files.hpp>

namespace BnZ {

inline void initEasyLoggingpp(int argc, char** argv) {
    START_EASYLOGGINGPP(argc, argv);

    if(exists("myeasylog.conf")) {
        el::Configurations conf("myeasylog.conf");
        el::Loggers::setDefaultConfigurations(conf, true);
    }
}

}
