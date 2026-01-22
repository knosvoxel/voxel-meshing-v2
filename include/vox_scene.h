#pragma once

#include <vector>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"

#include "vox_instance.h"

#include "timer.h"

using namespace glm;

typedef struct RotationData {
	vec4 instanceSize;
	vec4 rotatedSize;
	vec4 minBounds;
	mat4 transform;
};

struct VoxScene {
	VoxScene() {};
	~VoxScene() {};

	VoxScene& operator=(VoxScene&& other) noexcept {
		if (this != &other) {
			// Move data
			instances = std::move(other.instances);
			palette = other.palette;
			shader = std::move(other.shader);
			remapTo8sCompute = std::move(other.remapTo8sCompute);
			applyRotationsCompute = std::move(other.applyRotationsCompute);
			bufferSizeCompute = std::move(other.bufferSizeCompute);
			meshingCompute = std::move(other.meshingCompute);

			// Leave the other object in a valid state
			other.palette = 0;
		}
		return *this;
	}

	void load(const char* path);
	void render(mat4 mvp, float currentFrame);
	void cleanup();

	uint32 createRotatedModelBuffer(const ogt_vox_scene* scene, uint32 instanceIdx, ComputeShader& compute, ivec3& rotatedSize, float64& dispatchDuration);

	std::vector<VoxInstance> instances;

	uint32 palette = 0, voxelCount = 0, vertexCount = 0, numInstances = 0;

	float64 total_draw_call_duration = 0.0;

	Shader shader;
	ComputeShader remapTo8sCompute, applyRotationsCompute, bufferSizeCompute, meshingCompute;
};