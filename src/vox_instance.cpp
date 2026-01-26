#include "vox_instance.h"

#include "timer.h"

void VoxInstance::generateMesh(uint32& totalVertexCount, uint32 modelSSBO, uint32 meshingSSBO_V, uint32 meshingSSBO_I, vec3 offset, ivec3 modelSize, ComputeShader& meshingX, ComputeShader& meshingY, ComputeShader& meshingZ, float64& dispatchDuration, float64& dispatchPre, float64& dispatchPost)
{
    Timer timer;
    timer.start();
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, meshingSSBO_V); // temp buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, meshingSSBO_I); // temp buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indirectCommand);

    InstanceData instanceData{};
    instanceData.instanceDimensions = modelSize;
    instanceData.offset = offset;

    glCreateBuffers(1, &instanceDataBuffer);
    glNamedBufferStorage(instanceDataBuffer, sizeof(InstanceData), &instanceData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, instanceDataBuffer);


    uint32 meshingQuery;
    glGenQueries(1, &meshingQuery);
    glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    roundedSizeX = (modelSize.x + 15) / 16;
    roundedSizeY = (modelSize.y + 15) / 16;
    roundedSizeZ = (modelSize.z + 15) / 16;
    timer.stop();
    dispatchPre = timer.elapsedMilliseconds();

    meshingX.use();
    glDispatchCompute(roundedSizeY, roundedSizeZ, 1);

    meshingY.use();
    glDispatchCompute(roundedSizeX, roundedSizeZ, 1);

    meshingZ.use();
    glDispatchCompute(roundedSizeX, roundedSizeY, 1);

    glEndQuery(GL_TIME_ELAPSED);

    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
        GL_ELEMENT_ARRAY_BARRIER_BIT |
        GL_COMMAND_BARRIER_BIT
    );
    timer.start();
    DrawElementsIndirectCommand commandData;
    glGetNamedBufferSubData(indirectCommand, 0, sizeof(DrawElementsIndirectCommand), &commandData);
    uint32 indexCount = commandData.count;
    uint32 vertexCount = (indexCount / 6) * 4;

    totalVertexCount += vertexCount;

    int32 available = 0;
    while (!available) {
        glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    }

    uint64 elapsedGPU = 0;
    glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    // dispatch time in us
    dispatchDuration = elapsedGPU / 1000;

    glDeleteQueries(1, &meshingQuery);

    if (vertexCount > 0) {
        glCreateBuffers(1, &vbo);
        glNamedBufferStorage(vbo, sizeof(Vertex) * vertexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(meshingSSBO_V, vbo, 0, 0, sizeof(Vertex) * vertexCount);

        glCreateBuffers(1, &ibo);
        glNamedBufferStorage(ibo, sizeof(uint32) * indexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(meshingSSBO_I, ibo, 0, 0, sizeof(uint32) * indexCount);

        glCreateVertexArrays(1, &vao);

        glVertexArrayElementBuffer(vao, ibo);

        // position attribute in the vertex shader
        glEnableVertexArrayAttrib(vao, 0);
        glVertexArrayAttribBinding(vao, 0, 0);
        glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));

        // packed normal & color index data
        glEnableVertexArrayAttrib(vao, 1);
        glVertexArrayAttribBinding(vao, 1, 0);
        glVertexArrayAttribIFormat(vao, 1, 1, GL_UNSIGNED_INT, offsetof(Vertex, packedData));

        glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));
    }
    else {
        vao = 0;
        vbo = 0;
        ibo = 0;
    }
    timer.stop();
    dispatchPost = timer.elapsedMilliseconds();
}

void VoxInstance::render()
{
    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectCommand);
    //glDrawArraysIndirect(GL_TRIANGLES, 0);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

void VoxInstance::cleanup()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
}