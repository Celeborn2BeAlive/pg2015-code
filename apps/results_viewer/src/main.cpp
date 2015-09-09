#include <iostream>

#include "ResultsViewer.hpp"

INITIALIZE_EASYLOGGINGPP

namespace BnZ {

int main(int argc, char** argv) {
    if(argc < 4) {
        std::cerr << "Usage: " << argv[0] << " < results directory path > < window width > < window height > < logging option >" << std::endl;
        return -1;
    }

    initEasyLoggingpp(argc, argv);

    auto windowWidth = lexicalCast<std::size_t>(argv[2]);
    auto windowHeight = lexicalCast<std::size_t>(argv[3]);

    ResultsViewer viewer(argv[0], argv[1], Vec2u(1600, 900));
    viewer.run();

	return 0;
}

}

int main(int argc, char** argv) {
#ifdef _DEBUG
        std::cerr << "DEBUG MODE ACTIVATED" << std::endl;
#endif

#ifdef DEBUG
        std::cerr << "DEBUG MODE ACTIVATED" << std::endl;
#endif
    return BnZ::main(argc, argv);
}
