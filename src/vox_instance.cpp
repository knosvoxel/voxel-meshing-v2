#include "vox_instance.h"

#include "timer.h"

void VoxInstance::generateMesh(uint32 modelSSBO, MeshingBuffers& buffers, MeshingShaders& shaders, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements)
{
    Timer timer;
    timer.start();

    model_size = modelSize;
    transform = translate(mat4(1.0f), worldOffset - vec3(floor(model_size.x / 2.0), floor(model_size.y / 2.0), floor(model_size.z / 2.0)));

    rotatedModelSSBO = modelSSBO;

    DrawElementsIndirectCommand indirectData{};
    indirectData.count = 0;
    indirectData.instanceCount = 1;
    indirectData.firstIndex = 0;
    indirectData.baseVertex = 0;
    indirectData.baseInstance = 0;

    glCreateBuffers(1, &indirectCommand);

    glNamedBufferStorage(indirectCommand, sizeof(DrawElementsIndirectCommand), &indirectData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    //compute shader call
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modelSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers.meshingSSBO_V); // temp buffer vertices
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers.meshingSSBO_I); // temp buffer indices
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffers.meshingSSBO_P); // temp buffer packedData: Bytes | 0: 00000000 | 1: 00000000 | 2: normal index |3: color index |
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indirectCommand);

    uint32 meshingQuery;
    glGenQueries(1, &meshingQuery);
    glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    roundedSizeX = (model_size.x + 15) / 16;
    roundedSizeY = (model_size.y + 15) / 16;
    roundedSizeZ = (model_size.z + 15) / 16;
    timer.stop();
    measurements.dispatchPre = timer.elapsedMilliseconds();

    shaders.meshingComputeX.use();
    shaders.meshingComputeX.setVec3("model_size", model_size);
    glDispatchCompute(roundedSizeY, roundedSizeZ, 1);

    shaders.meshingComputeY.use();
    shaders.meshingComputeY.setVec3("model_size", model_size);
    glDispatchCompute(roundedSizeX, roundedSizeZ, 1);

    shaders.meshingComputeZ.use();
    shaders.meshingComputeZ.setVec3("model_size", model_size);
    glDispatchCompute(roundedSizeX, roundedSizeY, 1);

    glEndQuery(GL_TIME_ELAPSED);

    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_COMMAND_BARRIER_BIT
    );
    timer.start();
    DrawElementsIndirectCommand commandData;
    glGetNamedBufferSubData(indirectCommand, 0, sizeof(DrawElementsIndirectCommand), &commandData);
    uint32 indexCount = commandData.count;
    uint32 vertexCount = (indexCount / 6) * 4;
    uint32 faceCount = indexCount / 6;

    measurements.vertexCount += vertexCount;
    measurements.indexCount += indexCount;
    measurements.packedDataCount += faceCount;

    int32 available = 0;
    while (!available) {
        glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    }

    uint64 elapsedGPU = 0;
    glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    // dispatch time in us
    measurements.meshGenerationDuration = elapsedGPU / 1000;

    glDeleteQueries(1, &meshingQuery);

    if (vertexCount > 0) {
        glCreateBuffers(1, &vertexSSBO);
        glNamedBufferStorage(vertexSSBO, sizeof(Vertex) * vertexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(buffers.meshingSSBO_V, vertexSSBO, 0, 0, sizeof(Vertex) * vertexCount);

        glCreateBuffers(1, &packedSSBO);
        glNamedBufferStorage(packedSSBO, sizeof(uint32) * faceCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(buffers.meshingSSBO_P, packedSSBO, 0, 0, sizeof(uint32) * faceCount);

        glCreateBuffers(1, &ibo);
        glNamedBufferStorage(ibo, sizeof(uint32) * indexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(buffers.meshingSSBO_I, ibo, 0, 0, sizeof(uint32) * indexCount);

        glCreateVertexArrays(1, &vao);
        glVertexArrayElementBuffer(vao, ibo);
    }
    else {
        vao = 0;
        vertexSSBO = 0;
        packedSSBO = 0;
        ibo = 0;
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