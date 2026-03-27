#pragma once

#include <omp.h>
#include <atomic>
#include <vector>
#include <array>
#include <bit>
#include <deque>
#include <memory>

#include <ext/matrix_transform.hpp>

#include "ogt_wrapper.h"
#include "compute.h"
#include "timer.h"
#include "shader.h"

using namespace glm;

enum class FaceDirection {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	FORWARD,
	BACK
};

typedef struct MeasurementData {
	float64 meshGenerationDuration;
	float64 dispatchPre; 
	float64 dispatchPost;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;
	uint32 packedDataCount = 0;
};

typedef struct DrawElementsIndirectCommand {
	uint32 count;
	uint32 instanceCount;
	uint32 firstIndex;
	int32 baseVertex;
	uint32 baseInstance;
};

struct GreedyQuad {
	ivec2 start_pos;
	uint32 width;
	uint32 height;
};

// TODO: draw elements directly

struct Chunk {
	mat4 worldTransform;
	
	static constexpr int32 sizeXZ = 32, sizeY = 32;
	static constexpr int32 num_voxels = sizeXZ * sizeXZ * sizeY;

	bool is_empty = true;

	uint8 voxel_data[num_voxels] = {};

	static int32 getIndex(int32 x, int32 y, int32 z) {
		return (x & 31) | ((z & 31) << 5) | ((y & 31) << 10);
	}
};

struct ChunkMesh {
	mat4 transform;
	std::vector<uint32> vertices;
	std::vector<uint32> indices;

	uint32 vao;
	uint32 vertexSSBO;
	uint32 ibo;
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
			indirectCommand = other.indirectCommand;
			instanceDataBuffer = other.instanceDataBuffer;

			// Leave the other object in a valid state
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

	std::vector<GreedyQuad> meshBinaryPlane(std::array<uint32, 32>& data);
	std::vector<uint32> generateVerticesFromFace(FaceDirection dir, const uint8* voxelData);
	std::vector<uint32> generateIndices(size_t vertex_count);
	ChunkMesh generateChunkMesh(uint8* voxel_data);
	std::vector<std::unique_ptr<Chunk>> generateChunks();
	void generateInstanceMesh(const uint8* voxelData, vec3 modelSize,
	vec3 worldOffset, MeasurementData& measurements);

	const int32 getPoolIndex(int32 cx, int32 cy, int32 cz)
	{
		return cx + cz * sizeInChunks.x + cy * sizeInChunks.x * sizeInChunks.z;
	}

	void render(Shader& shader, mat4& mvp);

	void cleanup();

	const uint8* voxelData;
	ivec3 instanceDimensions;
	ivec3 sizeInChunks;
	ivec3 worldOffset;

	static const int32 chunk_size = 32;

	std::vector<std::unique_ptr<Chunk>> chunkData;
	std::vector<ChunkMesh> meshes;

	uint32 //vbo = 0, 
		vao = 0, 
		rotatedModelSSBO = 0, indirectCommand = 0, instanceDataBuffer = 0,
		roundedSizeX = 0, roundedSizeY = 0, roundedSizeZ = 0;
};