#include "vox_instance.h"

// mask to remove padding bits from uint64
static constexpr uint64 P_MASK = ~(1ull << 63 | 1ull);

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

static inline uint8 getVoxel(const uint8* voxels, int32 x, int32 y, int32 z, const ivec3& size)
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= size.x || y >= size.y || z >= size.z)
        return 0; // treat out-of-bounds as air
    return voxels[y + x * size.y + z * size.y * size.x];
}

static inline uint64 makeQuad(int32 x, int32 y, int32 z, int32 width, int32 height, uint8 color, uint8 face)
{
    return ((uint64)face << 40)
        | ((uint64)color << 32)
        | ((uint64)height << 24)
        | ((uint64)width << 18)
        | ((uint64)z << 12)
        | ((uint64)y << 6)
        | (uint64)x;
}

void VoxInstance::meshBinaryPlane(uint64* plane, int32 axis, int32 layer, FaceDirection dir, int32 negated_axis_offset, ivec3 chunk_offset, std::vector<uint64>& quads)
{
    for (uint32 row = 0; row < CHUNK_SIZE; row++)
    {
        uint64 bits = plane[row];

        while (bits != 0) // while unmeshed bits remain in row
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
                if (getVoxel(voxelData, next_pos.x, next_pos.y, next_pos.z, instanceDimensions) != color) break; // if colors don't match, terminate loop
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

            uint64 quad;
            switch (axis)
            {
            case 0: case 1: // Y faces: layer = y, row = x, col = z
                quad = makeQuad(row, layer + negated_axis_offset, col, width, height, color, axis);
                break;
            case 2: case 3: // X faces: layer = x, row = z, col = y
                quad = makeQuad(layer + negated_axis_offset, col, row, width, height, color, axis);
                break;
            default: // Z faces: layer = z, row = x, col = y
                quad = makeQuad(row, col, layer + negated_axis_offset, width, height, color, axis);
                break;
            }
            quads.push_back(quad);
        }
    }
}

ChunkMesh VoxInstance::generateChunkMeshData(ivec3 chunk_offset, ChunkMeasurements& chunk_measurements)
{
    Timer local;
    local.start();
    ChunkMesh mesh{};
    // binary represenation of all solid voxels in the chunk
    static thread_local uint64 opaqueMask[CHUNK_SIZE_P * CHUNK_SIZE_P];
    memset(opaqueMask, 0, sizeof(uint64) * CHUNK_SIZE_P * CHUNK_SIZE_P);

    // cull mask to perform face meshing on, based on solids from opaqueMask
    // uint64: columns | CHUNK_SIZE * CHUNK_SIZE: rows * layers | 6: one per each axis
    static thread_local uint64 faceMasks[CHUNK_SIZE * CHUNK_SIZE * 6];
    memset(faceMasks, 0, sizeof(uint64) * CHUNK_SIZE * CHUNK_SIZE * 6);


    // binary representation for every solid voxel in mesh
    for (int32 z = 0; z < CHUNK_SIZE_P; z++) // layer
    for (int32 x = 0; x < CHUNK_SIZE_P; x++) // row
    for (int32 y = 0; y < CHUNK_SIZE_P; y++) // column
    {
        ivec3 pos = ivec3(x, y, z) + chunk_offset - ivec3(1);
        uint8 col = getVoxel(voxelData, pos.x, pos.y, pos.z, instanceDimensions);
        if (col != 0) {
            opaqueMask[x + y * CHUNK_SIZE_P] |= 1ull << z;
        }
    }

    local.stop();
    chunk_measurements.occupancyMaskTotal = local.elapsedMilliseconds();

    local.start();
    // face culling | a: layer b: row
    for (int32 a = 1; a < CHUNK_SIZE_P - 1; a++)
    for (int32 b = 1; b < CHUNK_SIZE_P - 1; b++)
    {
        // fetch colums of current 62 * 62 opaque mask layer
        const uint64 columnBits = opaqueMask[(a * CHUNK_SIZE_P) + b] & P_MASK;
        // index opaque mask in two directions
        const int baIndex = (b - 1) + (a - 1) * CHUNK_SIZE;
        const int abIndex = (a - 1) + (b - 1) * CHUNK_SIZE;

        // Y faces (up/down): a=y, b=x, bits=z
        faceMasks[baIndex + 0 * CHUNK_SIZE_2] = (columnBits & ~opaqueMask[(a * CHUNK_SIZE_P) + CHUNK_SIZE_P + b]) >> 1;
        faceMasks[baIndex + 1 * CHUNK_SIZE_2] = (columnBits & ~opaqueMask[(a * CHUNK_SIZE_P) - CHUNK_SIZE_P + b]) >> 1;

        // X faces (left/right): a=y, b=x, bits=z  
        faceMasks[abIndex + 2 * CHUNK_SIZE_2] = (columnBits & ~opaqueMask[(a * CHUNK_SIZE_P) + (b + 1)]) >> 1;
        faceMasks[abIndex + 3 * CHUNK_SIZE_2] = (columnBits & ~opaqueMask[(a * CHUNK_SIZE_P) + (b - 1)]) >> 1;

        // Z faces (forward/back): a=y, b=x, bits=z
        faceMasks[baIndex + 4 * CHUNK_SIZE_2] = (columnBits & ~(opaqueMask[(a * CHUNK_SIZE_P) + b] >> 1)) >> 1;
        faceMasks[baIndex + 5 * CHUNK_SIZE_2] = (columnBits & ~(opaqueMask[(a * CHUNK_SIZE_P) + b] << 1)) >> 1;
    }

    local.stop();
    chunk_measurements.faceCullingTotal = local.elapsedMilliseconds();

    local.start();

    // greedy meshing planes for every axis (6 directions) / face planes
    // 62 layers, 62 rows, 64 bit to fir the 62 column bits
    static thread_local uint64 face_planes[6][CHUNK_SIZE][CHUNK_SIZE];
    memset(face_planes, 0, sizeof(face_planes));

    // set bits whre faces are to be meshed
    for (int32 axis = 0; axis < 6; axis++)
    for (int i = 0; i < CHUNK_SIZE; i++)
    for (int j = 0; j < CHUNK_SIZE; j++)
    {
        uint64 col = faceMasks[i + j * CHUNK_SIZE + axis * CHUNK_SIZE_2];
        //// removes the right most padding value, because it's outside of the chunk/ not meshed
        //// skip padding by adding + 1 to x & z
        //uint64 col = col_face_masks[axis][z + 1][x + 1] >> 1;
        //// removes the left most padding value, because it's outside of the chunk/ not meshed
        //col = col & ~(1ull << uint64(CHUNK_SIZE));

        // fetch face positions
        // col is only 0 if no bits are left in the associated column
        while (col != 0) {
            int32 layer = std::countr_zero(col);
            // clear least significant set bit so that next iteration can clear next bit
            col &= col - 1;
            //face_planes[axis][y][x] |= 1ull << z;
            switch (axis)
            {
            case 0: case 1: face_planes[axis][j][i] |= 1ull << layer; break;
            case 2: case 3: face_planes[axis][j][layer] |= 1ull << i; break;
            case 4: case 5: face_planes[axis][layer][i] |= 1ull << j; break;
            }
        } 
    }

    std::vector<uint64> quads;
    quads.reserve(CHUNK_SIZE * CHUNK_SIZE);

    for (int32 axis = 0; axis < 6; axis++) {
        FaceDirection face_dir;
        switch (axis)
        {
        case 0: face_dir = FaceDirection::UP; break;
        case 1: face_dir = FaceDirection::DOWN; break;
        case 2: face_dir = FaceDirection::RIGHT; break;
        case 3: face_dir = FaceDirection::LEFT; break;
        case 4: face_dir = FaceDirection::BACK; break;
        default: face_dir = FaceDirection::FORWARD; break;
        }

        int32 negated_axis = negateAxis(face_dir);

        for (int32 layer = 0; layer < CHUNK_SIZE; layer++)
        {
            meshBinaryPlane(
                face_planes[axis][layer], axis, layer, face_dir, negated_axis, chunk_offset, quads
            );
        }
    }

    mesh.quads.insert(mesh.quads.end(), quads.begin(), quads.end());

    local.stop();
    chunk_measurements.meshingTotal = local.elapsedMilliseconds();

    return mesh;
}

void VoxInstance::generateMeshBuffers(MeasurementData& measurements)
{
    for (ChunkMesh& mesh : meshes)
    {
        if (mesh.quads.empty()) continue;
        firstVertices.push_back((int32)instanceQuads.size());
        vertexCounts.push_back((int32)mesh.quads.size() * 6);
        transforms.push_back(mesh.transform);
        instanceQuads.insert(instanceQuads.end(), mesh.quads.begin(), mesh.quads.end());
    }

    measurements.vertexCount += instanceQuads.size();
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
        ivec3 chunkOrigin = ivec3(cx, cy, cz) * CHUNK_SIZE;
        bool is_empty = true;

        // Check if any voxel in this chunk's region is solid
        for (int32 ly = 0; ly < CHUNK_SIZE && is_empty; ly++)
        for (int32 lz = 0; lz < CHUNK_SIZE && is_empty; lz++)
        for (int32 lx = 0; lx < CHUNK_SIZE && is_empty; lx++)
        {
            ivec3 voxel_pos = chunkOrigin + ivec3(lx, ly, lz);
            if (voxel_pos.x >= instanceDimensions.x ||
                voxel_pos.y >= instanceDimensions.y ||
                voxel_pos.z >= instanceDimensions.z)
                continue;

            uint32 idx = voxel_pos.y
                + voxel_pos.x * instanceDimensions.y
                + voxel_pos.z * instanceDimensions.y * instanceDimensions.x;
            if (voxelData[idx] != 0)
                is_empty = false;
        }

        if (!is_empty) {
            auto chunk = std::make_unique<Chunk>();
            chunk->worldTransform = translate(mat4(1.0f),
                vec3(worldOffset)
                + vec3(cx, cy, cz) * float32(CHUNK_SIZE)
                - vec3(floor(instanceDimensions.x / 2.0),
                    floor(instanceDimensions.y / 2.0),
                    floor(instanceDimensions.z / 2.0)));
            chunk->chunk_offset = chunkOrigin;

            int32 idx = getPoolIndex(cx, cy, cz);
            chunkData[idx] = std::move(chunk);
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
            meshes[i] = generateChunkMeshData(chunkData[i]->chunk_offset, localMeasurements[i]);
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
            [](const ChunkMesh& m) { return m.quads.empty(); }),
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