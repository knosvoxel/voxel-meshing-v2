#pragma once

#include <vector>
#include <algorithm>
#include <omp.h>
#include <chrono>

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
			applyRotationsCompute = std::move(other.applyRotationsCompute);

			// Leave the other object in a valid state
			other.palette = 0;
		}
		return *this;
	}

	void buildSceneBuffers();

	void load(const char* path);
	void render(mat4 mvp, float currentFrame);
	void cleanup();

	uint8* createRotatedModelCPU(const ogt_vox_scene* scene, uint32 instanceIdx, ivec3& rotatedModelSize, float64& dispatchDuration);

	uint32 sceneVAO;
	uint32 sceneVertexSSBO;
	uint32 sceneTransformSSBO;

	std::vector<VoxInstance> instances;

	std::vector<uint32> sceneVertices;
	std::vector<int32>  sceneFirsts;
	std::vector<int32>  sceneCounts;
	std::vector<mat4>   sceneTransforms;

	uint32 palette = 0, numInstances = 0;

	uint32 total_vertices = 0;
	
	MeasurementData measurements;

	Shader shader;
	ComputeShader applyRotationsCompute;
};