#include "benchmark_runner.h"

static std::string fmtDouble(double v, int decimals = 4)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(decimals) << v;
    return oss.str();
}


BenchmarkRunner::BenchmarkRunner(BenchmarkConfig cfg)
    : m_cfg(std::move(cfg))
{
    m_dxgi = createDXGIAdapter();
    if (!m_dxgi)
        std::cerr << "[bench] Warning: DXGI adapter unavailable, VRAM will be 0\n";

    // Make sure output directory exists
    fs::create_directories(m_cfg.outputDir);
}

BenchmarkRunner::~BenchmarkRunner()
{
    if (m_dxgi) { m_dxgi->Release(); m_dxgi = nullptr; }
}

void BenchmarkRunner::run()
{
    std::cout << "[Bench] Scene      : " << m_cfg.scenePath    << "\n";
    std::cout << "[Bench] Scenario   : " << static_cast<int>(m_cfg.scenario) << "\n";
    std::cout << "[Bench] Output dir : " << m_cfg.outputDir    << "\n\n";

    if (m_cfg.scenario == BenchScenario::Far)
        runPreprocessingPhase();
    runFramePhase();
}

void BenchmarkRunner::runPreprocessingPhase()
{
    const int  iterations    = m_cfg.preprocessIterations;
    const auto outPath  = makeOutputPath("preprocess_" + sceneToken() + ".csv");

    std::cout << "[Bench] Preprocessing phase: " << iterations
              << " iterations (iteration 0 is warmup)\n";
    std::cout << "[Bench] Writing to: " << outPath << "\n";

    std::ofstream csv(outPath);
    if (!csv) { std::cerr << "[Bench] ERROR: cannot open " << outPath << "\n"; return; }

    csv << "sep=,\n";

    // Header
    csv << "iteration"
        << ",total_preprocess_ms"
        << ",shader_load_ms"
        << ",scene_file_load_ms"
        << ",palette_overhead_ms"
        << ",rotation_total_ms"
        << ",rotation_compute_ms"
        << ",meshing_total_ms"
        << ",meshing_avg_us"
        << ",meshing_min_us"
        << ",meshing_max_us"
        << ",scene_buffer_build_ms"
        << "," << MemorySnapshot::csvHeader()
        << "\n";

    std::vector<double> totalMs;
    totalMs.reserve(iterations - 1);

    for (int iter = 0; iter < iterations; ++iter)
    {
        const bool isWarmup = (iter == 0);
        std::cout << "[Bench]   iter " << std::setw(2) << iter
                  << (isWarmup ? " (warmup, excluded from CSV)" : "") << " ... ";
        std::cout.flush();

        Application app;
        app.benchmarkMode = true; // suppresses interactive input
        app.overrideScenePath = m_cfg.scenePath;
        app.initWindow();
        app.initOpenGL();
        // no ImGui in benchmark mode

        // Sample memory just before load
        // (after GL context exists so VRAM baseline is established)
        MemorySnapshot memBefore = queryMemory(m_dxgi);

        Timer preprocessTimer;
        preprocessTimer.start();

        // load scene, populates app.scene.timings
        app.scene.load(m_cfg.scenePath.c_str());

        preprocessTimer.stop();
        double preprocessMs = preprocessTimer.elapsedMilliseconds();

        // Sample memory after load
        MemorySnapshot memAfter = queryMemory(m_dxgi);

        std::cout << std::fixed << std::setprecision(2) << preprocessMs << " ms\n";

        if (!isWarmup)
        {
            totalMs.push_back(preprocessMs);

            // Pull the detailed timing fields that scene.load() fills in
            const auto& t = app.scene.timings;

            csv << (iter - 1)                          // 0-based after warmup
                << "," << preprocessMs
                << "," << t.shaderLoadMs
                << "," << t.sceneFileLoadMs
                << "," << t.paletteOverheadMs
                << "," << t.rotationTotalMs
                << "," << t.rotationComputeMs
                << "," << t.meshingLoopMs
                << "," << t.meshingAvgUs
                << "," << t.meshingMinUs
                << "," << t.meshingMaxUs
                << "," << t.sceneBufferBuildMs
                << "," << memAfter.toCsvRow()
                << "\n";
        }

        app.scene.cleanup();
        app.cleanupWindow();
    }

    csv.flush();

    // Summary
    if (!totalMs.empty())
    {
        double avg = std::accumulate(totalMs.begin(), totalMs.end(), 0.0) / totalMs.size();
        double mn  = *std::min_element(totalMs.begin(), totalMs.end());
        double mx  = *std::max_element(totalMs.begin(), totalMs.end());
        std::cout << "[bench] Preprocessing summary over " << totalMs.size() << " runs:\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  avg=" << avg << " ms  min=" << mn << " ms  max=" << mx << " ms\n\n";
    }
}

void BenchmarkRunner::runFramePhase()
{
    const int    scenarioNum = static_cast<int>(m_cfg.scenario);
    const auto   outPath     = makeOutputPath(
        "frames_" + sceneToken() + "_scenario" + std::to_string(scenarioNum) + ".csv");

    std::cout << "[Bench] Frame phase: scenario " << scenarioNum
              << ", duration " << m_cfg.frameDurationSec << " s\n";
    std::cout << "[Bench] Writing to: " << outPath << "\n";

    std::ofstream csv(outPath);
    if (!csv) { std::cerr << "[Bench] ERROR: cannot open " << outPath << "\n"; return; }
    
    csv.imbue(std::locale::classic());

    csv << "sep=,\n";

    csv << "frame,frame_time_ms," << MemorySnapshot::csvHeader() << "\n";

    Application app;
    app.benchmarkMode = true;
    app.overrideScenePath = m_cfg.scenePath;
    app.initWindow();
    app.initOpenGL();

    app.scene.load(m_cfg.scenePath.c_str());

    if (m_cfg.scenario == BenchScenario::Path)
    {
        loadPaths(app.cameraPaths, m_cfg.camPathFile.c_str());
        CameraPath& path = app.cameraPaths[m_cfg.trackIndex];
        if (path.keyframes.size() < 2)
        {
            std::cerr << "[Bench] ERROR: track " << m_cfg.trackIndex
                      << " has fewer than 2 keyframes\n";
            return;
        }
        app.cam.Position = path.keyframes[0].pos;
        app.cam.Yaw      = path.keyframes[0].yaw;
        app.cam.Pitch    = path.keyframes[0].pitch;
        app.cam.updateCameraVectors();
        path.active       = true;
        path.currentIndex = 0;

        glm::vec3 diff = path.keyframes[1].pos - path.keyframes[0].pos;

        app.activePathIdx = m_cfg.trackIndex;
    }
    else // fixed camera
    {
        app.cam = Camera(m_cfg.camPos,
                         glm::vec3(0.f, 1.f, 0.f),
                         m_cfg.camYaw,
                         m_cfg.camPitch);
    }

    const double memSampleIntervalMs = 100.0;
    double nextMemSampleAtMs = 0.0;
    MemorySnapshot lastMem = queryMemory(m_dxgi);

    int    frameIndex   = 0;
    bool   firstFrame   = true;

    Timer loopTimer;
    Timer frameTimer;
    loopTimer.start();

    while (!glfwWindowShouldClose(app.window))
    {
        if (loopTimer.elapsedSeconds() >= m_cfg.frameDurationSec) break;

        frameTimer.start();

        // Memory sample (throttled)
        if (loopTimer.elapsedMilliseconds() >= nextMemSampleAtMs)
        {
            lastMem = queryMemory(m_dxgi);
            nextMemSampleAtMs = loopTimer.elapsedMilliseconds() + memSampleIntervalMs;
        }

        glClearColor(0.20f, 0.20f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(app.cam.Zoom),
            static_cast<float>(app.sizeX) / static_cast<float>(app.sizeY),
            0.1f, 10000.0f);
        glm::mat4 view  = app.cam.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 mvp   = projection * view * model;

        app.scene.render(mvp, static_cast<float>(glfwGetTime()));

        // Camera path update (scenario 3 only)
        if (m_cfg.scenario == BenchScenario::Path)
            app.updateCameraPath(static_cast<float>(frameTimer.elapsedSeconds()));

        glfwSwapBuffers(app.window);
        glfwPollEvents();

        frameTimer.stop();
        double frameMs = frameTimer.elapsedMilliseconds();

        // Skip frame 0
        if (firstFrame) { firstFrame = false; frameIndex = 0; continue; }

        csv << frameIndex
            << "," << std::fixed << std::setprecision(4) << frameMs
            << "," << lastMem.toCsvRow()
            << "\n";

        ++frameIndex;
    }

    csv.flush();
    std::cout << "[bench] Recorded " << frameIndex << " frames\n\n";

    // ---- cleanup ----
    app.scene.cleanup();
    app.cleanupWindow();
}

std::string BenchmarkRunner::makeOutputPath(const std::string& filename) const
{
    return (fs::path(m_cfg.outputDir) / filename).string();
}

std::string BenchmarkRunner::sceneToken() const
{
    return fs::path(m_cfg.scenePath).stem().string();
}
