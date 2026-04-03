#pragma once

#include <omp.h>
#include <atomic>
#include <vector>
#include <array>
#include <bit>
#include <deque>
#include <memory>
#include <unordered_map>

#include <ext/matrix_transform.hpp>

#include "ogt_wrapper.h"
#include "compute.h"
#include "timer.h"
#include "shader.h"

using namespace glm;

static const int32 CHUNK_SIZE = 62;
static const int32 CHUNK_SIZE_P = CHUNK_SIZE + 2;
static const int32 CHUNK_SIZE_P2 = CHUNK_SIZE_P * CHUNK_SIZE_P;
static const int32 CHUNK_SIZE_P3 = CHUNK_SIZE_P * CHUNK_SIZE_P * CHUNK_SIZE_P;

enum class FaceDirection {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	FORWARD,
	BACK
};

typedef struct ChunkMeasurements {
	float64 occupancyMaskTotal = 0.0;
	float64 faceCullingTotal = 0.0;
	float64 meshingTotal = 0.0;
};

typedef struct MeasurementData {
	// For current instance in ms
	float64 meshPre = 0.0;
	float64 meshInstanceChunks = 0.0;
	float64 meshPost = 0.0;
	float64 meshTotal = 0.0;

	uint32 vertexCount = 0;
	uint32 chunkCount = 0;

	ChunkMeasurements chunkMeasurements;
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
	
	static constexpr int32 sizeXZ = CHUNK_SIZE, sizeY = CHUNK_SIZE;
	static constexpr int32 num_voxels = sizeXZ * sizeXZ * sizeY;

	bool is_empty = true;

	uint8 voxel_data[num_voxels] = {};

	ivec3 chunk_offset;

	static int32 getIndex(int32 x, int32 y, int32 z) {
		return x + y * sizeXZ + z * sizeXZ * sizeY;
	}
};

struct ChunkMesh {
	mat4 transform;
	std::vector<uint32> vertices;
};

struct VoxInstance {
	VoxInstance() {};
	~VoxInstance() {};

	VoxInstance& operator=(VoxInstance&& other) noexcept {
		return *this;
	}
	VoxInstance(const VoxInstance&) = default;
	VoxInstance& operator=(const VoxInstance&) = default;
	VoxInstance(VoxInstance&& other) noexcept {
		*this = std::move(other);
	}

	std::vector<GreedyQuad> meshBinaryPlane(std::array<uint64, CHUNK_SIZE>& data);

	ChunkMesh generateChunkMeshData(uint8* voxel_data, ivec3 chunk_offset, ChunkMeasurements& chunk_measurements);
	void generateMeshBuffers(MeasurementData& measurements);
	
	void generateChunks();


	void generateInstanceMesh(const uint8* voxelData, vec3 modelSize,
	vec3 worldOffset, MeasurementData& measurements);

	const int32 getPoolIndex(int32 cx, int32 cy, int32 cz)
	{
		return cx + cz * sizeInChunks.x + cy * sizeInChunks.x * sizeInChunks.z;
	}

	const uint8* voxelData;
	ivec3 instanceDimensions;
	ivec3 sizeInChunks;
	ivec3 worldOffset;

	std::vector<std::unique_ptr<Chunk>> chunkData;
	std::vector<ChunkMesh> meshes;

	std::vector<uint32> instanceVertices;
	std::vector<int32> firsts;
	std::vector<int32> counts;
	std::vector<mat4> transforms;
};