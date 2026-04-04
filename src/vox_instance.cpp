#include "vox_instance.h"

static inline int32 negateAxis(FaceDirection& dir) {
    switch (dir)
    {
    case FaceDirection::UP: return 1; break;
    case FaceDirection::DOWN: return 0; break;
    case FaceDirection::LEFT: return 0; break;
    case FaceDirection::RIGHT: return 1; break;
    case FaceDirection::FORWARD: return 0; break;
    case FaceDirection::BACK: return 1; break;
    default: return -1; break;
    }
}

static ivec3 worldToSample(FaceDirection dir, int32 axis, int32 x, int32 y)
{
    switch (dir)
    {
    case FaceDirection::UP: case FaceDirection::DOWN:
        return ivec3(x, axis, y); break;
    case FaceDirection::LEFT: case FaceDirection::RIGHT:
        return ivec3(axis, y, x); break;
    case FaceDirection::FORWARD: case FaceDirection::BACK:
        return ivec3(x, y, axis); break;
    default: return ivec3(-1); break;
    }
}

static uint32 getNormalIndex(FaceDirection dir)
{
    switch (dir)
    {
    case FaceDirection::UP: return 5; break;
    case FaceDirection::DOWN: return 4; break;
    case FaceDirection::LEFT: return 0; break;
    case FaceDirection::RIGHT: return 1; break;
    case FaceDirection::FORWARD: return 2; break;
    case FaceDirection::BACK: return 3; break;
    default: return -1; break;
    }
}

static bool isReverseOrder(FaceDirection dir) {
    switch (dir)
    {
    case FaceDirection::UP: return true; break;
    case FaceDirection::DOWN: return false; break;
    case FaceDirection::LEFT: return false; break;
    case FaceDirection::RIGHT: return true; break;
    case FaceDirection::FORWARD: return true; break;
    case FaceDirection::BACK: return false;break;
    default: break;
    }
}

static inline uint8 getVoxel(const uint8* voxels, int32 x, int32 y, int32 z, const ivec3& size)
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= size.x || y >= size.y || z >= size.z)
        return 0; // treat out-of-bounds as air
    return voxels[x + y * size.x + z * size.x * size.y];
}

static inline uint32 makeVertex(ivec3 pos, uint32 normal, uint8 color) {
    return (uint32(pos.x) & 63u)
        | ((uint32(pos.y) & 63u) << 6)
        | ((uint32(pos.z) & 63u) << 12)
        | (normal << 18)
        | (uint32(color) << 22);
}

static void appendVertices(std::vector<uint32>& vertices, FaceDirection dir, uint32 axis, uint8 color, uint32 normal_idx, bool is_reverse, GreedyQuad& quad) {
    uint32 v1 = makeVertex(worldToSample(dir, axis, quad.start_pos.x, quad.start_pos.y), normal_idx, color);
    uint32 v2 = makeVertex(worldToSample(dir, axis, quad.start_pos.x + quad.width, quad.start_pos.y), normal_idx, color);
    uint32 v3 = makeVertex(worldToSample(dir, axis, quad.start_pos.x + quad.width, quad.start_pos.y + quad.height), normal_idx, color);
    uint32 v4 = makeVertex(worldToSample(dir, axis, quad.start_pos.x, quad.start_pos.y + quad.height), normal_idx, color);

    if (is_reverse) {
        vertices.insert(vertices.end(), { v1, v3, v2, v1, v4, v3 });
    }
    else {
        vertices.insert(vertices.end(), { v1, v2, v3, v1, v3, v4 });
    }
}

void VoxInstance::meshBinaryPlane(uint64* plane, int32 axis, int32 layer, FaceDirection dir, uint32 normal_idx, bool is_reverse, int32 negated_axis_offset, ivec3 chunk_offset, std::vector<uint32>& vertices)
{
    for (uint32 row = 0; row < CHUNK_SIZE; row++)
    {
        uint64 bits = plane[row];

        while (bits != 0)
        {
            int32 col = std::countr_zero(bits);

            ivec3 voxel_pos;
            switch (axis)
            {
            case 0: case 1: voxel_pos = ivec3(row, layer, col); break; // down | up
            case 2: case 3: voxel_pos = ivec3(layer, col, row); break; // left | right
            default: voxel_pos = ivec3(row, col, layer); break; // forward | back
            }
            voxel_pos += chunk_offset;
            uint8 color = getVoxel(voxelData, voxel_pos.x, voxel_pos.y, voxel_pos.z, instanceDimensions);

            uint64 height = 1;
            while (col + height < CHUNK_SIZE)
            {
                if (!(bits >> (col + height) & 1)) break;

                ivec3 next_pos;
                switch (axis)
                {
                case 0: case 1: next_pos = ivec3(row, layer, col + height); break;
                case 2: case 3: next_pos = ivec3(layer, col + height, row); break;
                default: next_pos = ivec3(row, col + height, layer);  break;
                }
                next_pos += chunk_offset;
                if (getVoxel(voxelData, next_pos.x, next_pos.y, next_pos.z, instanceDimensions) != color) break;
                height++;
            }

            uint64 height_mask = (height == 64) ? ~0ull : ((1ull << height) - 1ull);
            uint64 mask = height_mask << col;

            uint64 width = 1;
            while (row + width < CHUNK_SIZE) {
                if ((plane[row + width] >> col & height_mask) != height_mask) break;

                bool color_match = true;
                for (uint64 h = 0; h < height; h++)
                {
                    ivec3 next_pos;
                    switch (axis)
                    {
                    case 0: case 1: next_pos = ivec3(row + width, layer, col + h); break;
                    case 2: case 3: next_pos = ivec3(layer, col + h, row + width); break;
                    default: next_pos = ivec3(row + width, col + h, layer); break;
                    }
                    next_pos += chunk_offset;
                    if (getVoxel(voxelData, next_pos.x, next_pos.y, next_pos.z, instanceDimensions) != color) {
                        color_match = false;
                        break;
                    }
                }
                if (!color_match) break;

                // remove next row bits
                plane[row + width] &= ~mask;
                width++;
            }
            // remove current row bits
            bits &= ~mask;

            GreedyQuad quad{
                ivec2(row, col), (uint32)width, (uint32)height
            };
            appendVertices(vertices, dir, layer + negated_axis_offset, color, normal_idx, is_reverse, quad);
        }
    }
}

ChunkMesh VoxInstance::generateChunkMeshData(uint8* voxel_data, ivec3 chunk_offset, ChunkMeasurements& chunk_measurements)
{
    Timer local;
    local.start();
    ChunkMesh mesh{};
    // solid voxels as binary for each x, y, z axis
    uint64 axis_cols[3][CHUNK_SIZE_P][CHUNK_SIZE_P] = {};
    // cull mask to perform greedy slicing on, based on solids from axis_cols
    uint64 col_face_masks[6][CHUNK_SIZE_P][CHUNK_SIZE_P] = {};

    // binary representation for every solid voxel in y,x,z axis
    for (int32 y = 0; y < CHUNK_SIZE_P; y++)
    for (int32 z = 0; z < CHUNK_SIZE_P; z++)
    for (int32 x = 0; x < CHUNK_SIZE_P; x++)
    {
        ivec3 pos = ivec3(x, y, z) + chunk_offset - ivec3(1);
        uint8 col = getVoxel(voxelData, pos.x, pos.y, pos.z, instanceDimensions);
        if (col != 0) {
            // x,z : y axis
            axis_cols[0][z][x] |= 1ull << y;
            // z,y : x axis
            axis_cols[1][y][z] |= 1ull << x;
            // x,y : z axis
            axis_cols[2][y][x] |= 1ull << z;
        }
    }

    local.stop();
    chunk_measurements.occupancyMaskTotal = local.elapsedMilliseconds();

    local.start();
    // face culling
    for (int32 axis = 0; axis < 3; axis++) 
    for (int32 z = 0; z < CHUNK_SIZE_P; z++)
    for (int32 x = 0; x < CHUNK_SIZE_P; x++)
    {
        uint64 col = axis_cols[axis][z][x];
        // sample ascending axis, set true if air meets solid
        col_face_masks[axis * 2 + 1][z][x] = col & ~(col >> 1);
        // sample descending axis, set true if air meets solid
        col_face_masks[axis * 2 + 0][z][x] = col & ~(col << 1);
    }

    local.stop();
    chunk_measurements.faceCullingTotal = local.elapsedMilliseconds();

    local.start();
    // greedy meshing planes for every axis (6 directions)
    // key(color) -> unordered_map<axis(0 - 32), binary_plane(CHUNK_SIZE x CHUNK_SIZE bits)>
    //std::unordered_map<uint32, std::unordered_map<uint32, std::array<uint64, CHUNK_SIZE>>> data[6];
    uint64 face_planes[6][CHUNK_SIZE][CHUNK_SIZE] = {};

    // find faces and build binary planes based on the voxel color in y direction
    for (int32 axis = 0; axis < 6; axis++)
    for (int z = 0; z < CHUNK_SIZE; z++)
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        // removes the right most padding value, because it's outside of the chunk/ not meshed
        // skip padding by adding + 1 to x & z
        uint64 col = col_face_masks[axis][z + 1][x + 1] >> 1;
        // removes the left most padding value, because it's outside of the chunk/ not meshed
        col = col & ~(1ull << uint64(CHUNK_SIZE));

        // fetch face positions
        while (col != 0) {
            int32 y = std::countr_zero(col);
            // clear least significant set bit
            col &= col - 1;
            face_planes[axis][y][x] |= 1ull << z;
        } 
    }

    std::vector<uint32> vertices;
    vertices.reserve(CHUNK_SIZE * CHUNK_SIZE * 6);

    for (int32 axis = 0; axis < 6; axis++) {
        FaceDirection face_dir;
        switch (axis)
        {
        case 0: face_dir = FaceDirection::DOWN; break;
        case 1: face_dir = FaceDirection::UP; break;
        case 2: face_dir = FaceDirection::LEFT; break;
        case 3: face_dir = FaceDirection::RIGHT; break;
        case 4: face_dir = FaceDirection::FORWARD; break;
        default: face_dir = FaceDirection::BACK; break;
        }

        uint32 normal_idx = getNormalIndex(face_dir);
        bool is_reverse = isReverseOrder(face_dir);
        int32 negated_axis = negateAxis(face_dir);

        for (int32 layer = 0; layer < CHUNK_SIZE; layer++)
        {
            meshBinaryPlane(
                face_planes[axis][layer], axis, layer, face_dir, normal_idx, is_reverse, negated_axis, chunk_offset, vertices
            );
        }
    }

    mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());

    local.stop();
    chunk_measurements.meshingTotal = local.elapsedMilliseconds();

    return mesh;
}

void VoxInstance::generateMeshBuffers(MeasurementData& measurements)
{
    for (ChunkMesh& mesh : meshes)
    {
        if (mesh.vertices.empty()) continue;
        firstVertices.push_back((int32)instanceVertices.size());
        vertexCounts.push_back((int32)mesh.vertices.size());
        transforms.push_back(mesh.transform);
        instanceVertices.insert(instanceVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    }

    measurements.vertexCount += instanceVertices.size();
}

void VoxInstance::generateChunks()
{
    int32 total_chunks = sizeInChunks.x * sizeInChunks.y * sizeInChunks.z;
    chunkData.resize(total_chunks);

#pragma omp parallel for collapse(3) schedule(static)
    for (int32 cy = 0; cy < sizeInChunks.y; cy++)
    for (int32 cz = 0; cz < sizeInChunks.z; cz++)
    for (int32 cx = 0; cx < sizeInChunks.x; cx++)
    {
        Chunk local{};

        for (int32 ly = 0; ly < local.sizeY; ly++)
        for (int32 lz = 0; lz < local.sizeXZ; lz++)
        for (int32 lx = 0; lx < local.sizeXZ; lx++)
        {
            ivec3 voxel_pos = ivec3(cx, cy, cz) * CHUNK_SIZE + ivec3(lx, ly, lz);

            if (voxel_pos.x >= instanceDimensions.x ||
                voxel_pos.y >= instanceDimensions.y ||
                voxel_pos.z >= instanceDimensions.z)
                continue;

            uint32 col_idx = voxel_pos.x + voxel_pos.y * instanceDimensions.x + voxel_pos.z * instanceDimensions.x * instanceDimensions.y;
            uint8 col = voxelData[col_idx];
            if (col != 0) {
                local.voxel_data[Chunk::getIndex(lx, ly, lz)] = col;
                local.is_empty = false;
            }
        }

        if (!local.is_empty) {
            local.worldTransform = translate(mat4(1.0f), vec3(worldOffset) + vec3(cx, cy, cz) * float32(CHUNK_SIZE) - vec3(floor(instanceDimensions.x / 2.0), floor(instanceDimensions.y / 2.0), floor(instanceDimensions.z / 2.0)));

            local.chunk_offset = ivec3(cx, cy, cz) * CHUNK_SIZE;

            int32 idx = getPoolIndex(cx, cy, cz);
            chunkData[idx] = std::make_unique<Chunk>(local);
        }
    }
}

void VoxInstance::generateInstanceMesh(const uint8* voxelData, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements)
{
    Timer timer, timerTotal;
    timer.start();
    timerTotal.start();
    instanceDimensions = ivec3(modelSize);
    this->voxelData = voxelData;
    this->worldOffset = worldOffset;

    sizeInChunks = ivec3((instanceDimensions + CHUNK_SIZE - 1) / CHUNK_SIZE);

    generateChunks();
    
    timer.stop();
    measurements.meshPre += timer.elapsedMilliseconds();
    measurements.chunkCount += chunkData.size();

    timer.start();
    meshes.resize(chunkData.size());

    std::vector<ChunkMeasurements> localMeasurements{};
    localMeasurements.resize(chunkData.size());

#pragma omp parallel for schedule(static)
    for (int32 i = 0; i < (int32)chunkData.size(); i++)
    {
        if (chunkData[i] != nullptr)
        {
            meshes[i] = generateChunkMeshData(chunkData[i]->voxel_data, chunkData[i]->chunk_offset, localMeasurements[i]);
            meshes[i].transform = chunkData[i]->worldTransform;
        }
    }

    measurements.actuallyMeshedChunkCount = std::count_if(
        chunkData.begin(), chunkData.end(),
        [](const auto& c) {return c != nullptr; }
    );

    timer.stop();
    measurements.meshInstanceChunks = timer.elapsedMilliseconds();

    timer.start();
    meshes.erase(
        std::remove_if(meshes.begin(), meshes.end(), 
            [](const ChunkMesh& m) { return m.vertices.empty(); }),
        meshes.end()
    );

    generateMeshBuffers(measurements);

    timer.stop();
    measurements.meshPost = timer.elapsedMilliseconds();
    measurements.meshTotal = timerTotal.elapsedMilliseconds();

    for (size_t i = 0; i < localMeasurements.size(); i++)
    {
        measurements.chunkMeasurements.occupancyMaskTotal += localMeasurements[i].occupancyMaskTotal;
        measurements.chunkMeasurements.faceCullingTotal += localMeasurements[i].faceCullingTotal;
        measurements.chunkMeasurements.meshingTotal += localMeasurements[i].meshingTotal;
    }
}