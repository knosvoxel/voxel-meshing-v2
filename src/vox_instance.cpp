#include "vox_instance.h"

#include "timer.h"

void VoxInstance::generateMesh(uint32 modelSSBO, MeshingBuffers& buffers, MeshingShaders& shaders, vec3 modelSize, vec3 worldOffset, MeasurementData& measurements)
{
    Timer timer;
    timer.start();

    model_size = modelSize;
    transform = translate(mat4(1.0f), worldOffset - vec3(floor(model_size.x / 2.0), floor(model_size.y / 2.0), floor(model_size.z / 2.0)));

    rotatedModelSSBO = modelSSBO;

    DrawArraysIndirectCommand indirectData{};
    indirectData.count = 0;
    indirectData.instanceCount = 1;
    indirectData.first = 0;
    indirectData.baseInstance = 0;

    glCreateBuffers(1, &indirectCommand);
    glNamedBufferStorage(indirectCommand, sizeof(DrawArraysIndirectCommand),
        &indirectData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    // bindings: 0 = voxel data, 1 = quad staging, 2 = indirect
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modelSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers.meshingSSBO_Q);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectCommand);

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

    // Readback
    DrawArraysIndirectCommand commandData;
    glGetNamedBufferSubData(indirectCommand, 0, sizeof(DrawArraysIndirectCommand), &commandData);
    uint32 quadCount = commandData.count;
    measurements.quadCount += quadCount;

    int32 available = 0;
    while (!available) {
        glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    }

    uint64 elapsedGPU = 0;
    glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    // dispatch time in us
    measurements.meshGenerationDuration = elapsedGPU / 1000;

    glDeleteQueries(1, &meshingQuery);

    if (quadCount > 0) {
        glCreateBuffers(1, &quadSSBO);
        glNamedBufferStorage(quadSSBO, sizeof(Quad) * quadCount,
            nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(buffers.meshingSSBO_Q, quadSSBO,
            0, 0, sizeof(Quad) * quadCount);

        glCreateVertexArrays(1, &vao);
    }
    else {
        vao = 0;
        quadSSBO = 0;
    }
    timer.stop();
    measurements.dispatchPost = timer.elapsedMilliseconds();
}

void VoxInstance::render()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, quadSSBO);
    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectCommand);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &quadSSBO);
    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
    glDeleteBuffers(1, &rotatedModelSSBO);
    free(voxelData);
}