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

struct InstanceRange {
	uint32 quadOffset;
	uint32 quadCount;
	mat4   transform;
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

	void createRotatedModelBuffer(const ogt_vox_scene* scene, uint32 instanceIdx, uint32& rotatedModelSSBO, ComputeShader& compute, ivec3& rotatedSize, float64& dispatchDuration);
	uint8* createRotatedModelCPU(const ogt_vox_scene* scene, uint32 instanceIdx, ivec3& rotatedModelSize, float64& dispatchDuration);

	void buildSceneBuffers(const std::vector<InstanceRange>& ranges);

	std::vector<VoxInstance> instances;
	std::vector<InstanceRange> instance_ranges;

	MeshingBuffers buffers;
	uint32 palette = 0, numInstances = 0;

	float64 total_draw_call_duration = 0.0;
	

	uint32 sceneVAO = 0;
	uint32 sceneQuadSSBO = 0;
	uint32 sceneTransformSSBO = 0;
	uint32 sceneIndirectBuffer = 0;  // GL_DRAW_INDIRECT_BUFFER

	uint32 totalQuads = 0;
	uint32 numDrawCmds = 0;

	MeasurementData measurements;

	MeshingShaders meshingShaders;
	Shader shader;
	ComputeShader applyRotationsCompute;
};