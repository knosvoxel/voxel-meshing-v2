#include "vox_instance.h"

static inline uint8 getVoxel(const uint8* voxels, uint32 x, uint32 y, uint32 z, const ivec3& size) 
{
    return voxels[x + y * size.x + z * size.x * size.y];
}

static void createFaceX(const vec3& start, const vec3& end, uint32 color, uint32 normal_idx, MeshBuffers& buffer, std::atomic<uint32>& counter) 
{
    float32 face_length = end.x - start.x;

    uint32 base_idx = counter.fetch_add(6, std::memory_order_relaxed);
    uint32 face_idx = base_idx / 6;
    uint32 base_vtx = face_idx * 4;

    buffer.packedData[face_idx] = (color & 255) | (normal_idx << 8);

    float32 z_offset = (normal_idx == 3) ? 1.0f : 0.0f;
    vec3 base = start + vec3(0.0f, 0.0f, z_offset);

    vec3 pos0 = base;
    vec3 pos1 = base + vec3(0.0f, 1.0f, 0.0f);
    vec3 pos2 = base + vec3(1.0f + face_length, 1.0f, 0.0f);
    vec3 pos3 = base + vec3(1.0f + face_length, 0.0f, 0.0f);

    buffer.vertices[base_vtx + 0] = { detail::toFloat16(pos0.x), detail::toFloat16(pos0.y) , detail::toFloat16(pos0.z) };
    buffer.vertices[base_vtx + 1] = { detail::toFloat16(pos1.x), detail::toFloat16(pos1.y) , detail::toFloat16(pos1.z) };
    buffer.vertices[base_vtx + 2] = { detail::toFloat16(pos2.x), detail::toFloat16(pos2.y) , detail::toFloat16(pos2.z) };
    buffer.vertices[base_vtx + 3] = { detail::toFloat16(pos3.x), detail::toFloat16(pos3.y) , detail::toFloat16(pos3.z) };

    if (normal_idx == 3) {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 3;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 1;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
    else {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 1;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 3;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
}

static void createFaceY(const vec3& start, const vec3& end, uint32 color, uint32 normal_idx, MeshBuffers& buffer, std::atomic<uint32>& counter)
{
    float32 face_length = end.y - start.y;

    uint32 base_idx = counter.fetch_add(6, std::memory_order_relaxed);
    uint32 face_idx = base_idx / 6;
    uint32 base_vtx = face_idx * 4;

    buffer.packedData[face_idx] = (color & 255) | (normal_idx << 8);

    float32 x_offset = (normal_idx == 1) ? 1.0f : 0.0f;
    vec3 base = start + vec3(x_offset, 0.0f, 0.0f);

    vec3 pos0 = base;
    vec3 pos1 = base + vec3(0.0f, 1.0f + face_length, 0.0f);
    vec3 pos2 = base + vec3(0.0f, 1.0f + face_length, 1.0f);
    vec3 pos3 = base + vec3(0.0f, 0.0f, 1.0f);

    buffer.vertices[base_vtx + 0] = { detail::toFloat16(pos0.x), detail::toFloat16(pos0.y) , detail::toFloat16(pos0.z) };
    buffer.vertices[base_vtx + 1] = { detail::toFloat16(pos1.x), detail::toFloat16(pos1.y) , detail::toFloat16(pos1.z) };
    buffer.vertices[base_vtx + 2] = { detail::toFloat16(pos2.x), detail::toFloat16(pos2.y) , detail::toFloat16(pos2.z) };
    buffer.vertices[base_vtx + 3] = { detail::toFloat16(pos3.x), detail::toFloat16(pos3.y) , detail::toFloat16(pos3.z) };

    if (normal_idx == 1) {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 1;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 3;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
    else {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 3;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 1;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
}

static void createFaceZ(const vec3& start, const vec3& end, uint32 color, uint32 normal_idx, MeshBuffers& buffer, std::atomic<uint32>& counter)
{
    float32 face_length = end.z - start.z;

    uint32 base_idx = counter.fetch_add(6, std::memory_order_relaxed);
    uint32 face_idx = base_idx / 6;
    uint32 base_vtx = face_idx * 4;

    buffer.packedData[face_idx] = (color & 255) | (normal_idx << 8);

    float32 y_offset = (normal_idx == 5) ? 1.0f : 0.0f;
    vec3 base = start + vec3(0.0f, y_offset, 0.0f);

    vec3 pos0 = base;
    vec3 pos1 = base + vec3(1.0f, 0.0f, 0.0f);
    vec3 pos2 = base + vec3(1.0f, 0.0f, 1.0f + face_length);
    vec3 pos3 = base + vec3(0.0f, 0.0f, 1.0f + face_length);

    buffer.vertices[base_vtx + 0] = { detail::toFloat16(pos0.x), detail::toFloat16(pos0.y) , detail::toFloat16(pos0.z) };
    buffer.vertices[base_vtx + 1] = { detail::toFloat16(pos1.x), detail::toFloat16(pos1.y) , detail::toFloat16(pos1.z) };
    buffer.vertices[base_vtx + 2] = { detail::toFloat16(pos2.x), detail::toFloat16(pos2.y) , detail::toFloat16(pos2.z) };
    buffer.vertices[base_vtx + 3] = { detail::toFloat16(pos3.x), detail::toFloat16(pos3.y) , detail::toFloat16(pos3.z) };

    if (normal_idx == 5) {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 3;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 1;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
    else {
        buffer.indices[base_idx + 0] = base_vtx + 0;
        buffer.indices[base_idx + 1] = base_vtx + 2;
        buffer.indices[base_idx + 2] = base_vtx + 1;
        buffer.indices[base_idx + 3] = base_vtx + 0;
        buffer.indices[base_idx + 4] = base_vtx + 3;
        buffer.indices[base_idx + 5] = base_vtx + 2;
    }
}

void VoxInstance::sliceX(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter)
{
    const ivec3 size = ivec3(instanceData.modelSize);
    const vec3 origin = instanceData.worldOffset - vec3(floor(instanceData.modelSize * 0.5f));

#pragma omp parallel for collapse(2) schedule(dynamic)
    for (int32 iy = 0; iy < size.y; ++iy)
    {
        for (int32 iz = 0; iz < size.z; ++iz)
        {
            vec3 strip_base = origin + vec3(0.0f, float32(iy), float32(iz));

            uint32 prev_col_neg = 0, prev_col_pos = 0;
            vec3 start_neg{}, start_pos{};

            for (int32 ix = 0; ix < size.x; ++ix)
            {
                uint8 curr = getVoxel(voxels, ix, iy, iz, size);

                uint32 col_neg = 0, col_pos = 0;
                if (curr) {
                    if (iz == 0 || !getVoxel(voxels, ix, iy, iz - 1, size)) col_neg = curr;
                    if (iz == size.z - 1 || !getVoxel(voxels, ix, iy, iz + 1, size)) col_pos = curr;
                }

                vec3 current_pos = strip_base + vec3(float32(ix), 0.0f, 0.0f);

                if (col_neg != prev_col_neg) {
                    if (prev_col_neg) createFaceX(start_neg, current_pos - vec3(1, 0, 0), prev_col_neg, 2, buffer, counter);
                    if (col_neg) start_neg = current_pos;
                    prev_col_neg = col_neg;
                }

                if (col_pos != prev_col_pos) {
                    if (prev_col_pos) createFaceX(start_pos, current_pos - vec3(1, 0, 0), prev_col_pos, 3, buffer, counter);
                    if (col_pos) start_pos = current_pos;
                    prev_col_pos = col_pos;
                }
            }

            vec3 end_pos = strip_base + vec3(float32(size.x - 1), 0.0f, 0.0f);
            if (prev_col_neg) createFaceX(start_neg, end_pos, prev_col_neg, 2, buffer, counter);
            if (prev_col_pos) createFaceX(start_pos, end_pos, prev_col_pos, 3, buffer, counter);
        }
    }
}

void VoxInstance::sliceY(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter)
{
    const ivec3 size = ivec3(instanceData.modelSize);
    const vec3 origin = instanceData.worldOffset - vec3(floor(instanceData.modelSize * 0.5f));

#pragma omp parallel for collapse(2) schedule(dynamic)
    for (int32 ix = 0; ix < size.x; ++ix)
    {
        for (int32 iz = 0; iz < size.z; ++iz)
        {
            vec3 strip_base = origin + vec3(float32(ix), 0.0f, float32(iz));

            uint32 prev_col_neg = 0, prev_col_pos = 0;
            vec3 start_neg{}, start_pos{};

            for (int32 iy = 0; iy < size.y; ++iy)
            {
                uint8 curr = getVoxel(voxels, ix, iy, iz, size);

                uint32 col_neg = 0, col_pos = 0;
                if (curr) {
                    if (ix == 0 || !getVoxel(voxels, ix - 1, iy, iz, size)) col_neg = curr;
                    if (ix == size.x - 1 || !getVoxel(voxels, ix + 1, iy, iz, size)) col_pos = curr;
                }

                vec3 current_pos = strip_base + vec3(0.0f, float32(iy), 0.0f);

                if (col_neg != prev_col_neg) {
                    if (prev_col_neg) createFaceY(start_neg, current_pos - vec3(0, 1, 0), prev_col_neg, 0, buffer, counter);
                    if (col_neg) start_neg = current_pos;
                    prev_col_neg = col_neg;
                }

                if (col_pos != prev_col_pos) {
                    if (prev_col_pos) createFaceY(start_pos, current_pos - vec3(0, 1, 0), prev_col_pos, 1, buffer, counter);
                    if (col_pos) start_pos = current_pos;
                    prev_col_pos = col_pos;
                }
            }

            vec3 end_pos = strip_base + vec3(0.0f, float32(size.y - 1), 0.0f);
            if (prev_col_neg) createFaceY(start_neg, end_pos, prev_col_neg, 0, buffer, counter);
            if (prev_col_pos) createFaceY(start_pos, end_pos, prev_col_pos, 1, buffer, counter);
        }
    }
}

void VoxInstance::sliceZ(const uint8* voxels, const InstanceData& instanceData, MeshBuffers& buffer, std::atomic<uint32>& counter)
{
    const ivec3 size = ivec3(instanceData.modelSize);
    const vec3 origin = instanceData.worldOffset - vec3(floor(instanceData.modelSize * 0.5f));

#pragma omp parallel for collapse(2) schedule(dynamic)
    for (int32 ix = 0; ix < size.x; ++ix)
    {
        for (int32 iy = 0; iy < size.y; ++iy)
        {
            vec3 strip_base = origin + vec3(float32(ix), float32(iy), 0.0f);

            uint32 prev_col_neg = 0, prev_col_pos = 0;
            vec3 start_neg{}, start_pos{};

            for (int32 iz = 0; iz < size.z; ++iz)
            {
                uint8 curr = getVoxel(voxels, ix, iy, iz, size);

                uint32 col_neg = 0, col_pos = 0;
                if (curr) {
                    if (iy == 0 || !getVoxel(voxels, ix, iy - 1, iz, size)) col_neg = curr;
                    if (iy == size.y - 1 || !getVoxel(voxels, ix, iy + 1, iz, size)) col_pos = curr;
                }

                vec3 current_pos = strip_base + vec3(0.0f, 0.0f, float32(iz));

                if (col_neg != prev_col_neg) {
                    if (prev_col_neg) createFaceZ(start_neg, current_pos - vec3(0, 0, 1), prev_col_neg, 4, buffer, counter);
                    if (col_neg) start_neg = current_pos;
                    prev_col_neg = col_neg;
                }

                if (col_pos != prev_col_pos) {
                    if (prev_col_pos) createFaceZ(start_pos, current_pos - vec3(0, 0, 1), prev_col_pos, 5, buffer, counter);
                    if (col_pos) start_pos = current_pos;
                    prev_col_pos = col_pos;
                }
            }

            vec3 end_pos = strip_base + vec3(0.0f, 0.0f, float32(size.z - 1));
            if (prev_col_neg) createFaceZ(start_neg, end_pos, prev_col_neg, 4, buffer, counter);
            if (prev_col_pos) createFaceZ(start_pos, end_pos, prev_col_pos, 5, buffer, counter);
        }
    }
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

    std::atomic<uint32> faceCounter{ 0 };


    timer.stop();
    measurements.dispatchPre = timer.elapsedMilliseconds();


    timer.start();
    sliceX(voxelData, instanceData, buffer, faceCounter);
    sliceY(voxelData, instanceData, buffer, faceCounter);
    sliceZ(voxelData, instanceData, buffer, faceCounter);
    timer.stop();

    measurements.meshGenerationDuration = timer.elapsedMilliseconds() / 1000.0;

    uint32 indexCount = faceCounter.load() * 6;
    uint32 vertexCount = faceCounter.load() * 4;
    uint32 faceCount = faceCounter.load();

    buffer.indirectCommand->count = indexCount;

    measurements.vertexCount += vertexCount;
    measurements.indexCount += indexCount;
    measurements.packedDataCount += faceCount;

    timer.start();

    if (vertexCount > 0) {
        glCreateBuffers(1, &vertexSSBO);
        glNamedBufferStorage(vertexSSBO, sizeof(Vertex) * vertexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(vertexSSBO, 0, sizeof(Vertex) * vertexCount, buffer.vertices);

        glCreateBuffers(1, &packedSSBO);
        glNamedBufferStorage(packedSSBO, sizeof(uint32) * faceCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(packedSSBO, 0, sizeof(uint32) * faceCount, buffer.packedData);

        glCreateBuffers(1, &ibo);
        glNamedBufferStorage(ibo, sizeof(uint32) * indexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferSubData(ibo, 0, sizeof(uint32) * indexCount, buffer.indices);

        glCreateBuffers(1, &indirectCommand);
        glNamedBufferStorage(indirectCommand, sizeof(DrawElementsIndirectCommand), buffer.indirectCommand, GL_DYNAMIC_STORAGE_BIT);

        glCreateVertexArrays(1, &vao);
        glVertexArrayElementBuffer(vao, ibo);
    }
    else {
        vao = 0;
        vertexSSBO = 0;
        packedSSBO = 0;
        ibo = 0;
        indirectCommand = 0;
    }
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