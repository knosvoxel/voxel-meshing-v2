#pragma once

#include <omp.h>
#include <atomic>

#include "ogt_wrapper.h"
#include "compute.h"
#include "timer.h"

using namespace glm;

typedef struct InstanceData {
	vec3 modelSize;
	vec3 worldOffset;
};

typedef struct MeasurementData {
	float64 meshGenerationDuration;
	float64 dispatchPre; 
	float64 dispatchPost;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;
	uint32 packedDataCount = 0;
};

typedef struct Vertex {
	int16 x, y, z; // mainly used for buffer size, actually float16 in compute shaders
};

typedef struct DrawElementsIndirectCommand {
	uint32 count;
	uint32 instanceCount;
	uint32 firstIndex;
	int32 baseVertex;
	uint32 baseInstance;
};

struct MeshBuffers {
	Vertex* vertices;
	uint32* indices;
	uint32* packedData;
	DrawElementsIndirectCommand* indirectCommand;
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

	void sliceX(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter);
	void sliceY(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter);
	void sliceZ(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter);

	void generateMesh(const uint8* voxelData, MeshBuffers& buffer, InstanceData& instanceData, MeasurementData& measurements);

	void render();

	void cleanup();

	uint8* voxelData;

	uint32 //vbo = 0, 
		vao = 0, 
		ibo = 0, vertexSSBO = 0, packedSSBO = 0, rotatedModelSSBO = 0, indirectCommand = 0, instanceDataBuffer = 0,
		roundedSizeX = 0, roundedSizeY = 0, roundedSizeZ = 0;
};