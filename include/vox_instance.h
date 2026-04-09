#pragma once

#include <ext/matrix_transform.hpp>

#include "ogt_wrapper.h"
#include "compute.h"

using namespace glm;

typedef struct InstanceData {
	vec3 modelSize;
	vec3 worldOffset;
};

typedef struct MeshingBuffers {
	uint32 meshingSSBO_Q;
};

typedef struct MeshingShaders {
	ComputeShader meshingComputeX;
	ComputeShader meshingComputeY;
	ComputeShader meshingComputeZ;
};

typedef struct MeasurementData {
	float64 meshGenerationDuration;
	float64 dispatchPre; 
	float64 dispatchPost;
	uint32 quadCount = 0;
};

typedef struct Quad {
	float32 x, y, z; // mainly used for buffer size, actually float16 in compute shaders
	uint32 packedData; // 4 Bytes: | 16 bits Length | 5 bits unused | 3 bits Normal | 8 bits Color |
};

typedef struct DrawArraysIndirectCommand {
	uint32 count;
	uint32 instanceCount;
	uint32 first;
	uint32 baseInstance;
};

struct VoxInstance {
	VoxInstance() {};
	~VoxInstance() {};

	VoxInstance& operator=(VoxInstance&& other) noexcept {
		if (this != &other) {
			// Clean up existing resources
			cleanup();

			// Move data
			vao = other.vao;
			quadSSBO = other.quadSSBO;
			indirectCommand = other.indirectCommand;
			instanceDataBuffer = other.instanceDataBuffer;

			// Leave the other object in a valid state
			other.vao = 0;
			other.quadSSBO = 0;
			other.indirectCommand = 0;
			other.instanceDataBuffer = 0;
		}
		return *this;
	}
	VoxInstance(const VoxInstance&) = default;
	VoxInstance& operator=(const VoxInstance&) = default;
	VoxInstance(VoxInstance&& other) noexcept {
		*this = std::move(other);
	}

	void generateMesh(uint32 modelSSBO, MeshingBuffers& buffers, MeshingShaders& shaders, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements);

	void render();

	void cleanup();

	uint8* voxelData;

	vec3 model_size;
	mat4 transform;

	uint32 vao = 0, quadSSBO = 0, rotatedModelSSBO = 0, indirectCommand = 0, instanceDataBuffer = 0,
		roundedSizeX = 0, roundedSizeY = 0, roundedSizeZ = 0;
};