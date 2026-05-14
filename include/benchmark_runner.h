#pragma once
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>

#include "benchmark_config.h"
#include "memory_query.h"
#include "timer.h"
#include <string>
#include "application.h"

namespace fs = std::filesystem;

class BenchmarkRunner
{
public:
    explicit BenchmarkRunner(BenchmarkConfig cfg);
    ~BenchmarkRunner();

    void run();

private:
    void runPreprocessingPhase();
    void runFramePhase();

    std::string makeOutputPath(const std::string& filename) const;

    // Build the scene name token used in filenames (stem of the .vox path).
    std::string sceneToken() const;

    BenchmarkConfig  m_cfg;
    IDXGIAdapter3*   m_dxgi = nullptr;
};
