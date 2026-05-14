#include "application.h"
#include "benchmark_runner.h"

#include <cstring>
#include <iostream>

static bool hasFlag(int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}

int main(int argc, char** argv)
{
    if (hasFlag(argc, argv, "--bench"))
    {
        auto cfg = BenchmarkConfig::parse(argc, argv);
        if (!cfg) return EXIT_FAILURE;

        BenchmarkRunner runner(std::move(*cfg));
        runner.run();
        return EXIT_SUCCESS;
    }

    // Normal interactive run
    Application app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}