#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace glm;

struct CameraKeyframe {
	vec3 pos;
	float32 yaw;
	float32 pitch;
};

struct CameraPath {
	std::vector<CameraKeyframe> keyframes;
	int32 currentIndex = 0;
	bool active = false;
	float32 speed = 50.0f;
	float32 rotationSpeed = 90.0f;
};

inline void savePaths(const CameraPath paths[10], const std::string& filepath)
{
    nlohmann::json root;

    for (int i = 0; i < 10; i++)
    {
        nlohmann::json track;
        track["speed"] = paths[i].speed;
        track["rotationSpeed"] = paths[i].rotationSpeed;

        nlohmann::json keyframeArray = nlohmann::json::array();
        for (const auto& keyframe : paths[i].keyframes)
        {
            keyframeArray.push_back({
                {"px", keyframe.pos.x},
                {"py", keyframe.pos.y},
                {"pz", keyframe.pos.z},
                {"yaw",   keyframe.yaw},
                {"pitch", keyframe.pitch}
                });
        }
        track["keyframes"] = keyframeArray;
        root[std::to_string(i)] = track;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open path file for writing: " << filepath << std::endl;
        return;
    }
    file << root.dump(4);
    std::cout << "Paths saved to " << filepath << std::endl;
}

inline void loadPaths(CameraPath paths[10], const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open path file for reading: " << filepath << std::endl;
        return;
    }

    nlohmann::json root;
    try {
        file >> root;
    }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        std::string key = std::to_string(i);
        if (!root.contains(key)) continue;

        auto& track = root[key];
        paths[i].keyframes.clear();
        paths[i].speed = track.value("speed", 50.0f);
        paths[i].rotationSpeed = track.value("rotationSpeed", 90.0f);

        for (auto& keyframe : track["keyframes"])
        {
            CameraKeyframe frame;
            frame.pos = { keyframe["px"], keyframe["py"], keyframe["pz"] };
            frame.yaw = keyframe["yaw"];
            frame.pitch = keyframe["pitch"];
            paths[i].keyframes.push_back(frame);
        }
        std::cout << "Track " << i << ": loaded "
            << paths[i].keyframes.size() << " keyframes" << std::endl;
    }
}