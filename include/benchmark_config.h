#pragma once
#include <string>
#include <optional>
#include <iostream>
#include <glm/glm.hpp>

enum class BenchScenario
{
    Far = 1,  // scenario 1 or 2: static camera position
    Close = 2,
    Path = 3,  // scenario 3: animated camera path
};

struct BenchmarkConfig
{
    // required
    std::string  scenePath;          //.vox file path
    BenchScenario scenario = BenchScenario::Far;

    // camera settings for scenario 1 and 2
    glm::vec3    camPos = { 60.f, 200.f, 60.f };
    float        camYaw = 225.f;
    float        camPitch = -20.f;

    // camera settings for scenario 3
    std::string  camPathFile;        // .json with keyframe path
    int          trackIndex = 0;

    // output directory
    std::string  outputDir = ".";

    // benchmark settings
    int   preprocessIterations = 21; // first is warmup
    float frameDurationSec = 5.f;

    static std::optional<BenchmarkConfig> parse(int argc, char** argv)
    {
        BenchmarkConfig cfg;
        bool gotScene = false;
        bool gotScenario = false;

        auto usage = []() {
            std::cerr <<
                "Usage: voxel_meshing --bench\n"
                "  --scene   <path.vox>      (required)\n"
                "  --scenario <1|2|3>        (required)\n"
                "\n"
                "  Scenario 1/2 (fixed camera):\n"
                "  --cam-pos  <x> <y> <z>\n"
                "  --cam-yaw  <degrees>\n"
                "  --cam-pitch <degrees>\n"
                "\n"
                "  Scenario 3 (animated path):\n"
                "  --cam-path <paths.json>\n"
                "  --track    <index>        (default 0)\n"
                "\n"
                "  --output   <dir>          (default: current dir)\n"
                "  --iters    <n>            (default: 21, first is warmup)\n"
                "  --duration <seconds>      (default: 5.0)\n";
            };

        for (int i = 1; i < argc; ++i)
        {
            std::string a = argv[i];

            auto next = [&](const char* flag) -> const char* {
                if (i + 1 >= argc) {
                    std::cerr << "Missing argument after " << flag << "\n";
                    return nullptr;
                }
                return argv[++i];
                };

            if (a == "--bench") { /* consumed by caller */ }
            else if (a == "--scene") { if (auto v = next("--scene")) { cfg.scenePath = v; gotScene = true; } }
            else if (a == "--output") { if (auto v = next("--output")) { cfg.outputDir = v; } }
            else if (a == "--cam-path") { if (auto v = next("--cam-path")) { cfg.camPathFile = v; } }
            else if (a == "--track") { if (auto v = next("--track")) { cfg.trackIndex = std::stoi(v); } }
            else if (a == "--iters") { if (auto v = next("--iters")) { cfg.preprocessIterations = std::stoi(v); } }
            else if (a == "--duration") { if (auto v = next("--duration")) { cfg.frameDurationSec = std::stof(v); } }
            else if (a == "--cam-yaw") { if (auto v = next("--cam-yaw")) { cfg.camYaw = std::stof(v); } }
            else if (a == "--cam-pitch") { if (auto v = next("--cam-pitch")) { cfg.camPitch = std::stof(v); } }
            else if (a == "--scenario")
            {
                const char* v = next("--scenario");
                if (!v) return std::nullopt;
                int s = std::stoi(v);
                if (s != 1 && s != 2 && s != 3) {
                    std::cerr << "Scenario must be 1, 2, or 3\n";
                    return std::nullopt;
                }
                cfg.scenario = (s == 3) ? BenchScenario::Path
                    : (s == 2) ? BenchScenario::Close
                    : BenchScenario::Far;
                gotScenario = true;
            }
            else if (a == "--cam-pos")
            {
                if (i + 3 >= argc) { std::cerr << "Missing args after --cam-pos\n"; return std::nullopt; }
                cfg.camPos.x = std::stof(argv[++i]);
                cfg.camPos.y = std::stof(argv[++i]);
                cfg.camPos.z = std::stof(argv[++i]);
            }
            else
            {
                std::cerr << "Unknown argument: " << a << "\n";
                usage();
                return std::nullopt;
            }
        }

        if (!gotScene || !gotScenario) {
            std::cerr << "--scene and --scenario are required\n";
            usage();
            return std::nullopt;
        }

        if (cfg.scenario == BenchScenario::Path && cfg.camPathFile.empty()) {
            std::cerr << "--scenario 3 requires --cam-path\n";
            usage();
            return std::nullopt;
        }

        return cfg;
    }
};