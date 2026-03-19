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
    case UP:
        offset = ivec3(0, 1, 0);
        break;
    case DOWN:
        offset = ivec3(0, -1, 0);
        break;
    case LEFT:
        offset = ivec3(-1, 0, 0);
        break;
    case RIGHT:
        offset = ivec3(1, 0, 0);
        break;
    case FORWARD:
        offset = ivec3(0, 0, -1);
        break;
    case BACK:
        offset = ivec3(0, 0, 1);
        break;
    default:
        offset = ivec3(-1, -1, -1);
        break;
    }

    return getVoxel(voxels, x + offset.x, y + offset.y, z + offset.z, size);
}

std::vector<GreedyQuad> VoxInstance::meshBinaryPlane(std::array<uint32, 32>& data)
{
    std::vector<GreedyQuad> greedy_quads;
    for (int32 row = 0; row < data.size(); row++)
    {
        uint32 y = 0;
        while (y < 32) {
            // count trailing zero bits to find first solid voxel
            y += std::countl_zero(data[row] >> y);

            if (y >= 32) continue;

            uint32 height = std::countl_one(data[row] >> y);

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
                data[row + width] == data[row + width] & !mask;
                width += 1;
            }
            greedy_quads.push_back(GreedyQuad{
                ivec2(row, y),
                width,
                height
                });
        }
    }
    return greedy_quads;
}

static ivec3 worldToSample(FaceDirection dir, int32 axis, int32 x, int32 y)
{
    switch (dir)
    {
    case UP:
        return ivec3(x, axis + 1, y);
        break;
    case DOWN:
        return ivec3(x, axis, y);
        break;
    case LEFT:
        return ivec3(axis, y, x);
        break;
    case RIGHT:
        return ivec3(axis + 1, y, x);
        break;
    case FORWARD:
        return ivec3(x, y, axis);
        break;
    case BACK:
        return ivec3(x, y, axis + 1);
        break;
    default:
        return ivec3(-1);
        break;
    }
}

static void appendVertices(std::vector<uint32>& vertices, FaceDirection dir, uint32 axis, uint8 color) {

}

std::vector<uint32> VoxInstance::generateVerticesFromFace(FaceDirection dir, const uint8* voxelData)
{
    std::vector<uint32> vertices;
    uint32 size = 32;
    for (int32 axis = 0; axis < size; axis++)
    {
        for (int32 color = 1; color <= 255; color++) {
            // create binary grid
            std::array<uint32, 32> x_data;
            for (int32 i = 0; i < size * size; i++)
            {
                uint32 row = i % size;
                uint32 column = (i / size);
                ivec3 pos = worldToSample(dir, axis, row, column);
                uint8 current = getVoxel(voxelData, pos.x, pos.y, pos.z, instanceDimensions);
                uint8 neg_z = getVoxelInNegDir(dir, voxelData, pos.x, pos.y, pos.z, instanceDimensions);

                bool is_solid = current != 0 && neg_z == 0;
                x_data[row] = ((1 << column) * uint32(is_solid)) | x_data[row];
            }
            std::vector<GreedyQuad> quads_from_axis = meshBinaryPlane(x_data);
            for (GreedyQuad quad : quads_from_axis)
            {
                appendVertices(vertices, dir, axis, color);
            }
        }
    }

    return vertices;
}

void VoxInstance::generateMesh(const uint8* voxelData, MeshBuffers& buffer, InstanceData& instanceData, MeasurementData& measurements)
{
    Timer timer;
    timer.start();

    buffer.indirectCommand->count = 0;
    buffer.indirectCommand->instanceCount = 1;
    buffer.indirectCommand->firstIndex = 0;
    buffer.indirectCommand->baseVertex = 0;
    buffer.indirectCommand->baseInstance = 0;

    timer.stop();
    measurements.dispatchPre = timer.elapsedMilliseconds();

    instanceDimensions = instanceData.modelSize;

    timer.start();

    std::vector<Vertex> quads{};

    std::vector<Vertex> up_quads = generateVerticesFromFace(UP, voxelData);
    std::vector<Vertex> down_quads = generateVerticesFromFace(DOWN, voxelData);
    std::vector<Vertex> left_quads = generateVerticesFromFace(LEFT, voxelData);
    std::vector<Vertex> right_quads = generateVerticesFromFace(RIGHT, voxelData);
    std::vector<Vertex> forward_quads = generateVerticesFromFace(FORWARD, voxelData);
    std::vector<Vertex> back_quads = generateVerticesFromFace(BACK, voxelData);

    quads.insert(quads.end(), up_quads.begin(), up_quads.end());
    quads.insert(quads.end(), down_quads.begin(), down_quads.end());
    quads.insert(quads.end(), left_quads.begin(), up_quads.end());
    quads.insert(quads.end(), right_quads.begin(), right_quads.end());
    quads.insert(quads.end(), forward_quads.begin(), forward_quads.end());
    quads.insert(quads.end(), back_quads.begin(), back_quads.end());
    
    timer.stop();

    measurements.meshGenerationDuration = timer.elapsedMilliseconds() / 1000.0;

    //uint32 indexCount = faceCounter.load() * 6;
    //uint32 vertexCount = faceCounter.load() * 4;
    //uint32 faceCount = faceCounter.load();

    //buffer.indirectCommand->count = indexCount;

    //measurements.vertexCount += vertexCount;
    //measurements.indexCount += indexCount;
    //measurements.packedDataCount += faceCount;

    timer.start();

    //if (vertexCount > 0) {
    //    glCreateBuffers(1, &vertexSSBO);
    //    glNamedBufferStorage(vertexSSBO, sizeof(Vertex) * vertexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
    //    glNamedBufferSubData(vertexSSBO, 0, sizeof(Vertex) * vertexCount, buffer.vertices);

    //    glCreateBuffers(1, &packedSSBO);
    //    glNamedBufferStorage(packedSSBO, sizeof(uint32) * faceCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
    //    glNamedBufferSubData(packedSSBO, 0, sizeof(uint32) * faceCount, buffer.packedData);

    //    glCreateBuffers(1, &ibo);
    //    glNamedBufferStorage(ibo, sizeof(uint32) * indexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
    //    glNamedBufferSubData(ibo, 0, sizeof(uint32) * indexCount, buffer.indices);

    //    glCreateBuffers(1, &indirectCommand);
    //    glNamedBufferStorage(indirectCommand, sizeof(DrawElementsIndirectCommand), buffer.indirectCommand, GL_DYNAMIC_STORAGE_BIT);

    //    glCreateVertexArrays(1, &vao);
    //    glVertexArrayElementBuffer(vao, ibo);
    //}
    //else {
    //    vao = 0;
    //    vertexSSBO = 0;
    //    packedSSBO = 0;
    //    ibo = 0;
    //    indirectCommand = 0;
    //}
    timer.stop();
    measurements.dispatchPost = timer.elapsedMilliseconds();
}

void VoxInstance::render()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vertexSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, packedSSBO);

    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectCommand);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vertexSSBO);
    glDeleteBuffers(1, &packedSSBO);
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
    glDeleteBuffers(1, &rotatedModelSSBO);
    
    free(voxelData);
}