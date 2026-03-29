#include "vox_instance.h"

static inline uint8 getVoxel(const uint8* voxels, uint32 x, uint32 y, uint32 z, const ivec3& size) 
{
    return voxels[x + y * size.x + z * size.x * size.y];
}

static inline uint8 getVoxelInNegDir(FaceDirection dir, const uint8* voxels, uint32 x, uint32 y, uint32 z, const ivec3& size)
{
    ivec3 offset;

    switch (dir)
    {
    case FaceDirection::UP:
        offset = ivec3(0, 1, 0);
        break;
    case FaceDirection::DOWN:
        offset = ivec3(0, -1, 0);
        break;
    case FaceDirection::LEFT:
        offset = ivec3(-1, 0, 0);
        break;
    case FaceDirection::RIGHT:
        offset = ivec3(1, 0, 0);
        break;
    case FaceDirection::FORWARD:
        offset = ivec3(0, 0, -1);
        break;
    case FaceDirection::BACK:
        offset = ivec3(0, 0, 1);
        break;
    default:
        offset = ivec3(-1, -1, -1);
        break;
    }

    return getVoxel(voxels, x + offset.x, y + offset.y, z + offset.z, size);
}

static inline int32 negateAxis(FaceDirection& dir) {
    switch (dir)
    {
    case FaceDirection::UP:
        return -1;
        break;
    case FaceDirection::DOWN:
        return 0;
        break;
    case FaceDirection::LEFT:
        return 0;
        break;
    case FaceDirection::RIGHT:
        return -1;
        break;
    case FaceDirection::FORWARD:
        return 0;
        break;
    case FaceDirection::BACK:
        return 1;
        break;
    default:
        return -1;
        break;
    }
}

static inline uint32 makeVertex(ivec3 pos, uint32 normal, uint8 color) {
    return uint32{ pos.x | pos.y << 6 | pos.z << 12 | normal << 18 | uint32(color) << 22 };
}

std::vector<GreedyQuad> VoxInstance::meshBinaryPlane(std::array<uint32, 32>& data)
{
    std::vector<GreedyQuad> greedy_quads;
    for (int32 row = 0; row < data.size(); row++)
    {
        uint32 y = 0;
        while (y < 32) {
            // count trailing zero bits to find first solid voxel
            y += std::countr_zero(data[row] >> y);
            if (y >= 32) break;

            uint32 height = std::countr_one(data[row] >> y);

            // convert height value to equal amount of positive bits:
            // e.g. 1 = 0b1, 2 = 0b11, 4 = 0b1111, 8 = 0b11111111
            uint32 height_as_mask = height == 0 ? 0 : (std::rotl<uint32>(1, height) - 1);
            uint32 mask = height_as_mask << y;

            uint32 width = 1;
            // grow horizontally
            while (row + width < 32) {
                // fetch bits spanning height in next row
                uint32 next_row_height = (data[row + width] >> y) & height_as_mask;
                if (next_row_height != height_as_mask) {
                    break; // can't expand further
                }

                // remove bits we expanded into as each face can only be meshed once
                data[row + width] = data[row + width] & ~mask;
                width += 1;
            }
            greedy_quads.push_back(GreedyQuad{
                ivec2(row, y),
                width,
                height
                });

            y += height;
        }
    }
    return greedy_quads;
}

static ivec3 worldToSample(FaceDirection dir, int32 axis, int32 x, int32 y)
{
    switch (dir)
    {
    case FaceDirection::UP:
        return ivec3(x, axis + 1, y);
        break;
    case FaceDirection::DOWN:
        return ivec3(x, axis, y);
        break;
    case FaceDirection::LEFT:
        return ivec3(axis, y, x);
        break;
    case FaceDirection::RIGHT:
        return ivec3(axis + 1, y, x);
        break;
    case FaceDirection::FORWARD:
        return ivec3(x, y, axis);
        break;
    case FaceDirection::BACK:
        return ivec3(x, y, axis + 1);
        break;
    default:
        return ivec3(-1);
        break;
    }
}

static uint32 getNormalIndex(FaceDirection dir)
{
    switch (dir)
    {
    case FaceDirection::UP:
        return 0;
        break;
    case FaceDirection::DOWN:
        return 1;
        break;
    case FaceDirection::LEFT:
        return 2;
        break;
    case FaceDirection::RIGHT:
        return 3;
        break;
    case FaceDirection::FORWARD:
        return 4;
        break;
    case FaceDirection::BACK:
        return 5;
        break;
    default:
        return -1;
        break;
    }
}

static bool isReverseOrder(FaceDirection dir) {
    switch (dir)
    {
    case FaceDirection::UP:
        return true;
        break;
    case FaceDirection::DOWN:
        return false;
        break;
    case FaceDirection::LEFT:
        return false;
        break;
    case FaceDirection::RIGHT:
        return true;
        break;
    case FaceDirection::FORWARD:
        return true;
        break;
    case FaceDirection::BACK:
        return false;
        break;
    default:
        break;
    }
}

static void appendVertices(std::vector<uint32>& vertices, FaceDirection dir, uint32 axis, uint8 color, GreedyQuad& quad) {
    int32 negated_axis = negateAxis(dir);
    axis = axis + negated_axis;

    uint32 v1 = makeVertex(worldToSample(dir, axis, quad.start_pos.x, quad.start_pos.y), getNormalIndex(dir), color);
    uint32 v2 = makeVertex(worldToSample(dir, axis, quad.start_pos.x + quad.width, quad.start_pos.y), getNormalIndex(dir), color);
    uint32 v3 = makeVertex(worldToSample(dir, axis, quad.start_pos.x + quad.width, quad.start_pos.y + quad.height), getNormalIndex(dir), color);
    uint32 v4 = makeVertex(worldToSample(dir, axis, quad.start_pos.x, quad.start_pos.y + quad.height), getNormalIndex(dir), color);

    std::deque<uint32> new_vertices = { v1, v2, v3, v4 };

    if (isReverseOrder(dir)) {
        std::deque<uint32> tail(std::next(new_vertices.begin()), new_vertices.end());
        std::reverse(tail.begin(), tail.end());
        for (auto& v : tail)
        {
            new_vertices.push_back(v);
        }
    }

    vertices.insert(vertices.end(), new_vertices.begin(), new_vertices.end());
}

std::vector<uint32> VoxInstance::generateVerticesFromFace(FaceDirection dir, const uint8* voxelData)
{
    std::vector<uint32> vertices;
    uint32 size = 32;
    for (int32 axis = 0; axis < size; axis++)
    {
        for (int32 color = 1; color <= 255; color++) {
            // create binary grid
            std::array<uint32, 32> x_data{};

            for (int32 i = 0; i < size * size; i++)
            {
                uint32 row = i % size;
                uint32 column = (i / size);
                ivec3 pos = worldToSample(dir, axis, row, column);
                uint8 current = getVoxel(voxelData, pos.x, pos.y, pos.z, instanceDimensions);
                uint8 neg_z = getVoxelInNegDir(dir, voxelData, pos.x, pos.y, pos.z, instanceDimensions);

                if (current != color) continue;

                bool is_solid = current != 0 && neg_z == 0;
                x_data[row] = ((1 << column) * uint32(is_solid)) | x_data[row];
            }
            std::vector<GreedyQuad> quads_from_axis = meshBinaryPlane(x_data);
            for (GreedyQuad quad : quads_from_axis)
            {
                appendVertices(vertices, dir, axis, color, quad);
            }
        }
    }

    return vertices;
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

ChunkMesh VoxInstance::generateChunkMesh(uint8* voxel_data)
{
    std::cout << "A" << std::endl;

    ChunkMesh mesh{};
    std::vector<uint32> quads{};

    std::vector<uint32> up_quads = generateVerticesFromFace(FaceDirection::UP, voxel_data);
    std::vector<uint32> down_quads = generateVerticesFromFace(FaceDirection::DOWN, voxel_data);
    std::vector<uint32> left_quads = generateVerticesFromFace(FaceDirection::LEFT, voxel_data);
    std::vector<uint32> right_quads = generateVerticesFromFace(FaceDirection::RIGHT, voxel_data);
    std::vector<uint32> forward_quads = generateVerticesFromFace(FaceDirection::FORWARD, voxel_data);
    std::vector<uint32> back_quads = generateVerticesFromFace(FaceDirection::BACK, voxel_data);
    
    std::cout << "B" << std::endl;

    quads.insert(quads.end(), up_quads.begin(), up_quads.end());
    quads.insert(quads.end(), down_quads.begin(), down_quads.end());
    quads.insert(quads.end(), left_quads.begin(), left_quads.end());
    quads.insert(quads.end(), right_quads.begin(), right_quads.end());
    quads.insert(quads.end(), forward_quads.begin(), forward_quads.end());
    quads.insert(quads.end(), back_quads.begin(), back_quads.end());
    
    mesh.vertices.insert(mesh.vertices.end(), quads.begin(), quads.end());

    std::cout << "C" << std::endl;

    uint32 vertexSSBO;
    uint32 ibo;

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

    std::cout << "D" << std::endl;

    return mesh;
}

std::vector<std::unique_ptr<Chunk>> VoxInstance::generateChunks()
{
    int32 total_chunks = sizeInChunks.x * sizeInChunks.y * sizeInChunks.z;
    std::vector<std::unique_ptr<Chunk>> chunk_pool(total_chunks);

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
            ivec3 voxel_pos = ivec3(cx, cy, cz) * chunk_size + ivec3(lx, ly, lz);

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
            local.worldTransform = translate(mat4(1.0f), vec3(worldOffset) + vec3(cx, cy, cz) * 32.0f - vec3(floor(instanceDimensions.x / 2.0), floor(instanceDimensions.y / 2.0), floor(instanceDimensions.z / 2.0)));

            int32 idx = getPoolIndex(cx, cy, cz);
            chunk_pool[idx] = std::make_unique<Chunk>(local);
        }
    }

    return chunk_pool;
}

void VoxInstance::generateInstanceMesh(const uint8* voxelData, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements)
{
    instanceDimensions = ivec3(modelSize);
    this->voxelData = voxelData;
    this->worldOffset = worldOffset;

    sizeInChunks = ivec3((instanceDimensions + chunk_size - 1) / chunk_size);
    // worldTransform = 

    chunkData = generateChunks();

    std::vector<ChunkMesh> meshes;
    for (auto& chunk : chunkData) {
        meshes.push_back(generateChunkMesh(chunk->voxel_data));
    }
}

void VoxInstance::render(Shader& shader, mat4& mvp)
{
    for (ChunkMesh& mesh : meshes)
    {
        shader.setMat4("mvp", mvp * mesh.transform);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vertexSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ibo);

        glBindVertexArray(vao);
        //glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectCommand);
        glDrawElements(GL_TRIANGLES, mesh.vertices.size(), GL_UNSIGNED_INT, 0);
    }
    

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    glDeleteVertexArrays(1, &vao);

    for (ChunkMesh& mesh : meshes) {
        glDeleteBuffers(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vertexSSBO);
        glDeleteBuffers(1, &mesh.ibo);
        glDeleteBuffers(1, &mesh.ibo);
    }

    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
    glDeleteBuffers(1, &rotatedModelSSBO);
    
    //free(voxelData);
}