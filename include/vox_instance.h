#pragma once

#include "ogt_wrapper.h"
#include "compute.h"

using namespace glm;

typedef struct InstanceData {
	vec3 instanceDimensions;
	uint32 _pad1 = 0;
	vec3 offset;
	uint32 _pad2 = 0;
};

typedef struct Vertex {
	vec3 pos;
	uint32 packedData; // Bytes | 0: 00000000 | 1: 00000000 | 2: normal index |3: color index |
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
			if (vbo) glDeleteBuffers(1, &vbo);
			if (vao) glDeleteVertexArrays(1, &vao);

			// Move data
			vbo = other.vbo;
			vao = other.vao;
			indirectCommand = other.indirectCommand;
			instanceDataBuffer = other.instanceDataBuffer;

			// Leave the other object in a valid state
			other.vbo = 0;
			other.vao = 0;
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

	void generateMesh(uint32& totalVertexCount, uint32 modelSSBO, uint32 meshingSSBO, vec3 offset, ivec3 modelSize, ComputeShader& compute, float64& dispatchDuration);

	void render();

	void cleanup();

	uint32 vbo = 0, vao = 0, indirectCommand = 0, instanceDataBuffer = 0,
		roundedSizeX = 0, roundedSizeY = 0, roundedSizeZ = 0;
};