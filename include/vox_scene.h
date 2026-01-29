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
	//uint32 voxelCount = 0;
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
			applyRotationsCompute = std::move(other.applyRotationsCompute);
			buffers = other.buffers;
			meshingShaders = std::move(other.meshingShaders);

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

	MeshingBuffers buffers;
	uint32 palette = 0, numInstances = 0;

	float64 total_draw_call_duration = 0.0;
	
	MeasurementData measurements;

	MeshingShaders meshingShaders;
	Shader shader;
	ComputeShader applyRotationsCompute;
};