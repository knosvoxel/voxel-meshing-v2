#include "vox_instance.h"

void VoxInstance::prepareModelData(uint32 modelSSBO, vec4 offset, ivec3 modelSize, ComputeShader& compute)
{
    // round to closest multiple of 8
    roundedSizeX = (modelSize.x + 7) / 8 * 8;
    roundedSizeY = (modelSize.y + 7) / 8 * 8;
    roundedSizeZ = (modelSize.z + 7) / 8 * 8;

    glCreateBuffers(1, &remappedSSBO);

    glNamedBufferStorage(remappedSSBO, sizeof(uint8) * roundedSizeX * roundedSizeY * roundedSizeZ, nullptr, GL_DYNAMIC_STORAGE_BIT);
    glClearNamedBufferData(remappedSSBO, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); // all values are initially 0. 0 = empty voxel

    InstanceData instanceData{};
    instanceData.instanceSize = glm::vec4(modelSize, 0);
    instanceData.remappedSize = glm::vec4(roundedSizeX, roundedSizeY, roundedSizeZ, 0);
    instanceData.offset = offset;

    glCreateBuffers(1, &instanceDataBuffer);

    glNamedBufferStorage(instanceDataBuffer, sizeof(InstanceData), &instanceData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modelSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, remappedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceDataBuffer);

    compute.use();

    //uint32 meshingQuery;
    //glGenQueries(1, &meshingQuery);
    //glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    // remap_to_8s
    glDispatchCompute(modelSize.x, modelSize.y, modelSize.z);

    //glEndQuery(GL_TIME_ELAPSED);

    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT
    );

    //int32 available = 0;
    //while (!available) {
    //    glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    //}

    //GLuint64 elapsedGPU;
    //glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    //// dispatch time in us
    //dispatchDuration = elapsedGPU / 1000.0;
}

void VoxInstance::calculateBufferSize(uint32& voxelCount, ComputeShader& compute)
{
    glCreateBuffers(1, &vboSizeBuffer);

    glNamedBufferStorage(vboSizeBuffer, sizeof(GLuint), nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glClearNamedBufferData(vboSizeBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, remappedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vboSizeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceDataBuffer);

    compute.use();

    //GLuint meshingQuery;
    //glGenQueries(1, &meshingQuery);
    //glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    glDispatchCompute(roundedSizeX / 8, roundedSizeY / 8, roundedSizeZ / 8);

    //glEndQuery(GL_TIME_ELAPSED);

    // buffer_size_compute
    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT
    );

    //int32 available = 0;
    //while (!available) {
    //    glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    //}

    //GLuint64 elapsedGPU;
    //glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    //// dispatch time in us
    //dispatchDuration = elapsedGPU / 1000;

    void* vboSizePtr = glMapNamedBufferRange(vboSizeBuffer, 0, sizeof(GLuint),
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    if (vboSizePtr == nullptr) {
        std::cerr << "Failed to map vbo_size_buffer." << std::endl;
        return;
    }

    vboSize = *(GLuint*)vboSizePtr;

    glUnmapNamedBuffer(vboSizeBuffer);

    // Read back voxel count
    void* ptr = glMapNamedBuffer(instanceDataBuffer, GL_READ_ONLY);
    if (ptr) {
        InstanceData* instanceData = (InstanceData*)ptr;
        voxelCount += instanceData->voxelCount;
        glUnmapNamedBuffer(instanceDataBuffer);
    }

    glDeleteBuffers(1, &vboSizeBuffer);
}

void VoxInstance::generateMesh(uint32& vertexCount, ComputeShader& compute)
{
    DrawArraysIndirectCommand indirectData{};
    indirectData.count = 0;
    indirectData.instanceCount = 1;
    indirectData.first = 0;
    indirectData.baseInstance = 0;

    glCreateVertexArrays(1, &vao);

    glCreateBuffers(1, &vbo);
    glCreateBuffers(1, &indirectCommand);

    glNamedBufferStorage(vbo, sizeof(Vertex) * vboSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(indirectCommand, sizeof(DrawArraysIndirectCommand), &indirectData,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    // position attribute in the vertex shader
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);

    // packed normal & color index data
    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribBinding(vao, 1, 0);
    glVertexArrayAttribIFormat(vao, 1, 1, GL_UNSIGNED_INT, 3 * sizeof(GLfloat));

    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));

    //compute shader call
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, remappedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectCommand);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, instanceDataBuffer);

    compute.use();

    //GLuint meshingQuery;
    //glGenQueries(1, &meshingQuery);
    //glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    glDispatchCompute(roundedSizeX / 8, 1, roundedSizeZ / 8);

    //glEndQuery(GL_TIME_ELAPSED);

    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
        GL_COMMAND_BARRIER_BIT
    );

    // Read back vertex count
    void* ptr = glMapNamedBuffer(indirectCommand, GL_READ_ONLY);
    if (ptr) {
        DrawArraysIndirectCommand* commandData = (DrawArraysIndirectCommand*)ptr;
        vertexCount += commandData->count;
        glUnmapNamedBuffer(indirectCommand);
    }

    //GLint available = 0;
    //while (!available) {
    //    glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    //}

    //GLuint64 elapsedGPU;
    //glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    //// dispatch time in us
    //dispatchDuration = elapsedGPU / 1000;
}

void VoxInstance::render()
{
    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectCommand);
    glDrawArraysIndirect(GL_TRIANGLES, 0);

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &remappedSSBO);
    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
}