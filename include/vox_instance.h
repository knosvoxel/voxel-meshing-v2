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
	float32 x, y, z;
};

typedef struct DrawElementsIndirectCommand {
	uint32 count;
	uint32 instanceCount;
	uint32 firstIndex;
	int32 baseVertex;
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
			ibo = other.ibo;
			vertexSSBO = other.vertexSSBO;
			packedSSBO = other.packedSSBO;
			indirectCommand = other.indirectCommand;
			instanceDataBuffer = other.instanceDataBuffer;

			// Leave the other object in a valid state
			other.vao = 0;
			other.ibo = 0;
			other.vertexSSBO = 0;
			other.packedSSBO = 0;
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

	void generateMesh(uint32& totalVertexCount, uint32 modelSSBO, uint32 meshingSSBO_V, uint32 meshingSSBO_I, uint32 meshingSSBO_P, vec3 offset, ivec3 modelSize, ComputeShader& meshingX, ComputeShader& meshingY, ComputeShader& meshingZ, float64& dispatchDuration, float64& dispatchPre, float64& dispatchPost);

	void render();

	void cleanup();

	uint32 //vbo = 0, 
		vao = 0, 
		ibo = 0, vertexSSBO = 0, packedSSBO = 0, indirectCommand = 0, instanceDataBuffer = 0,
		roundedSizeX = 0, roundedSizeY = 0, roundedSizeZ = 0;
};