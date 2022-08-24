#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <exception>

int main(int argc, char** argv) {
    auto result = EXIT_FAILURE;

    try {
        doctest::Context context;
        context.setOption("no-path-filenames", true);
        context.applyCommandLine(argc, argv);
        result = context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return result;
}
