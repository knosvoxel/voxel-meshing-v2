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

    uint32 new_vertices[4] = { v1, v2, v3, v4 };

    if (is_reverse) {
        std::swap(new_vertices[1], new_vertices[3]);
    }
    vertices.insert(vertices.end(), new_vertices, new_vertices + 4);
}

std::vector<uint32> VoxInstance::generateIndices(size_t vertex_count)
{
    int32 indices_count = vertex_count / 4;
    std::vector<uint32> indices;
    indices.reserve(indices_count);
    for (size_t i = 0; i < indices_count; i++)
    {
        uint32 vert_index = static_cast<uint32>(i) * 4u;
        indices.push_back(vert_index);
        indices.push_back(vert_index + 1);
        indices.push_back(vert_index + 2);
        indices.push_back(vert_index);
        indices.push_back(vert_index + 2);
        indices.push_back(vert_index + 3);
    }

    return indices;
}

std::vector<GreedyQuad> VoxInstance::meshBinaryPlane(std::array<uint64, CHUNK_SIZE>& data)
{
    std::vector<GreedyQuad> greedy_quads;
    for (int32 row = 0; row < CHUNK_SIZE; row++)
    {
        uint64 y = 0;
        while (y < CHUNK_SIZE) {
            // count trailing zero bits to find first solid voxel
            y += std::countr_zero(data[row] >> y);
            if (y >= CHUNK_SIZE) break;

            uint64 height = std::countr_one(data[row] >> y);

            // convert height value to equal amount of positive bits:
            // e.g. 1 = 0b1, 2 = 0b11, 4 = 0b1111, 8 = 0b11111111
            uint64 height_as_mask = (height == CHUNK_SIZE_P) ? 0xFFFFFFFFFFFFFFFFull : ((1ull << height) - 1ull);
            uint64 mask = height_as_mask << y;

            uint64 width = 1;
            // grow horizontally
            while (row + width < CHUNK_SIZE) {
                // fetch bits spanning height in next row
                uint64 next_row_height = (data[row + width] >> y) & height_as_mask;
                if (next_row_height != height_as_mask) {
                    break; // can't expand further
                }

                // remove bits we expanded into as each face can only be meshed once
                data[row + width] &= ~mask;
                width += 1;
            }
            greedy_quads.push_back(GreedyQuad{
                ivec2(row, y),
                (uint32)width,
                (uint32)height
                });

            y += height;
        }
    }
    return greedy_quads;
}

ChunkMesh VoxInstance::generateChunkMesh(uint8* voxel_data, ivec3 chunk_offset)
{
    ChunkMesh mesh{};
    // solid voxels as binary for each x, y, z axis
    std::vector<uint64> axis_cols(3 * CHUNK_SIZE_P2, 0);
    // cull mask to perform greedy slicing on, based on solids from axis_cols
    std::vector<uint64> col_face_masks(3 * CHUNK_SIZE_P2 * 2, 0); // TODO: does this have to be uint64 or is uint8 sufficient?

    // binary representation for every solid voxel in y,x,z axis
    for (int32 y = 0; y < CHUNK_SIZE_P; y++)
    for (int32 z = 0; z < CHUNK_SIZE_P; z++)
    for (int32 x = 0; x < CHUNK_SIZE_P; x++)
    {
        ivec3 pos = ivec3(x, y, z) + chunk_offset - ivec3(1);
        uint8 col = getVoxel(voxelData, pos.x, pos.y, pos.z, instanceDimensions);
        if (col != 0) {
            // x,z : y axis
            axis_cols[x + (z * CHUNK_SIZE_P)] |= 1ull << uint64(y);
            // z,y : x axis
            axis_cols[z + (y * CHUNK_SIZE_P) + CHUNK_SIZE_P2] |= 1ull << uint64(x);
            // x,y : z axis
            axis_cols[x + (y * CHUNK_SIZE_P) + CHUNK_SIZE_P2 * 2] |= 1ull << uint64(z);
        }
    }

    // face culling
    for (int32 axis = 0; axis < 3; axis++) 
    for (int32 i = 0; i < CHUNK_SIZE_P2; i++)
    {
        uint64 col = axis_cols[(CHUNK_SIZE_P2 * axis) + i];
        // sample ascending axis, set true if air meets solid
        col_face_masks[(CHUNK_SIZE_P2 * (axis * 2 + 1)) + i] = col & ~(col >> 1);
        // sample descending axis, set true if air meets solid
        col_face_masks[(CHUNK_SIZE_P2 * (axis * 2 + 0)) + i] = col & ~(col << 1);
    }

    // greedy meshing planes for every axis (6 directions)
    // key(color) -> unordered_map<axis(0 - 32), binary_plane(CHUNK_SIZE x CHUNK_SIZE bits)>
    std::unordered_map<uint32, std::unordered_map<uint32, std::array<uint64, CHUNK_SIZE>>> data[6];

    // find faces and build binary planes based on the voxel color in y direction
    for (int32 axis = 0; axis < 6; axis++)
    for (int z = 0; z < CHUNK_SIZE; z++)
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        // skip padding by adding + 1 to x & z
        int32 col_idx = 1 + x + ((z + 1) * CHUNK_SIZE_P) + CHUNK_SIZE_P2 * axis;

        // removes the right most padding value, because it's outside of the chunk/ not meshed
        uint64 col = col_face_masks[col_idx] >> 1;
        // removes the left most padding value, because it's outside of the chunk/ not meshed
        col = col & ~(1ull << uint64(CHUNK_SIZE));

        // fetch face positions
        while (col != 0) {
            int32 y = std::countr_zero(col);
            // clear least significant set bit
            col &= col - 1;

            ivec3 voxel_pos = {};

            switch (axis)
            {
            case 0: case 1: voxel_pos = ivec3(x, y, z); break; // down, up
            case 2: case 3: voxel_pos = ivec3(y, z, x); break; // left, right
            default: voxel_pos = ivec3(x, z, y); break; // forward, back
            }
            
            voxel_pos += chunk_offset;

            uint8 current_voxel_col = getVoxel(voxelData, voxel_pos.x, voxel_pos.y, voxel_pos.z, instanceDimensions);

            data[axis][current_voxel_col][y][x] |= 1ull << z;
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

        auto& color_data = data[axis];

        for (auto& [color, axis_plane] : color_data)
        for (auto& [axis_pos, plane] : axis_plane)
        {
            std::vector<GreedyQuad> quads_from_axis = meshBinaryPlane(plane);

            for (GreedyQuad& quad : quads_from_axis)
            {
                appendVertices(vertices, face_dir, axis_pos + negated_axis, color, normal_idx, is_reverse, quad);
            }
        }
    }

    mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());

    uint32 vertexSSBO;
    uint32 ibo;
    uint32 vao;

    if (mesh.vertices.empty()) {
        vao = 0;
        vertexSSBO = 0;
        ibo = 0;
    }
    else {
        mesh.indices = generateIndices(mesh.vertices.size());

        glCreateBuffers(1, &vertexSSBO);
        glNamedBufferStorage(vertexSSBO, sizeof(uint32) * mesh.vertices.size(), nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(vertexSSBO, 0, sizeof(uint32) * mesh.vertices.size(), mesh.vertices.data());

        glCreateBuffers(1, &ibo);
        glNamedBufferStorage(ibo, sizeof(uint32) * mesh.indices.size(), nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(ibo, 0, sizeof(uint32) * mesh.indices.size(), mesh.indices.data());

        glCreateVertexArrays(1, &vao);
        glVertexArrayElementBuffer(vao, ibo);
    }

    mesh.vao = vao;
    mesh.vertexSSBO = vertexSSBO;
    mesh.ibo = ibo;

    return mesh;
}

void VoxInstance::generateChunks(std::vector<std::unique_ptr<Chunk>>& data)
{
    int32 total_chunks = sizeInChunks.x * sizeInChunks.y * sizeInChunks.z;
    data.resize(total_chunks);

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
            data[idx] = std::make_unique<Chunk>(local);
        }
    }
}

void VoxInstance::generateInstanceMesh(const uint8* voxelData, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements)
{
    instanceDimensions = ivec3(modelSize);
    this->voxelData = voxelData;
    this->worldOffset = worldOffset;

    sizeInChunks = ivec3((instanceDimensions + CHUNK_SIZE - 1) / CHUNK_SIZE);
    // worldTransform = 

    generateChunks(chunkData);

    for (auto& chunk : chunkData) {
        if (chunk != nullptr) {
            //ChunkMesh new_mesh = generateChunkMesh(chunk->voxel_data, chunk->chunk_offset);
            ChunkMesh new_mesh = generateChunkMesh(chunk->voxel_data, chunk->chunk_offset);
            new_mesh.transform = chunk->worldTransform;
            meshes.push_back(new_mesh);
        }
    }
}

void VoxInstance::render(Shader& shader, mat4& mvp)
{
    for (ChunkMesh& mesh : meshes)
    {
        shader.setMat4("mvp", mvp * mesh.transform);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vertexSSBO);

        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    for (ChunkMesh& mesh : meshes) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vertexSSBO);
        glDeleteBuffers(1, &mesh.ibo);
        glDeleteBuffers(1, &mesh.ibo);
    }

    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
    glDeleteBuffers(1, &rotatedModelSSBO);
    
    //free(voxelData);
}