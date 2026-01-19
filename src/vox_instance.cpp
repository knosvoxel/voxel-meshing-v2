#include "vox_instance.h"

void VoxInstance::generateMesh(uint32& totalVertexCount, uint32 modelSSBO, uint32 meshingSSBO, vec3 offset, ivec3 modelSize, ComputeShader& compute, float64& dispatchDuration)
{
    DrawArraysIndirectCommand indirectData{};
    indirectData.count = 0;
    indirectData.instanceCount = 1;
    indirectData.first = 0;
    indirectData.baseInstance = 0;

    InstanceData instanceData{};
    instanceData.instanceDimensions = modelSize;
    instanceData.offset = offset;

    glCreateBuffers(1, &indirectCommand);
    glCreateBuffers(1, &instanceDataBuffer);

    glNamedBufferStorage(indirectCommand, sizeof(DrawArraysIndirectCommand), &indirectData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
    glNamedBufferStorage(instanceDataBuffer, sizeof(InstanceData), &instanceData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    //compute shader call
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modelSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, meshingSSBO); // temp buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectCommand);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, instanceDataBuffer);

    compute.use();

    uint32 meshingQuery;
    glGenQueries(1, &meshingQuery);
    glBeginQuery(GL_TIME_ELAPSED, meshingQuery);

    roundedSizeX = (modelSize.x + 7) / 8 * 8;
    roundedSizeY = (modelSize.y + 7) / 8 * 8;
    roundedSizeZ = (modelSize.z + 7) / 8 * 8;

    glDispatchCompute(roundedSizeX / 8, 1, roundedSizeZ / 8);

    glEndQuery(GL_TIME_ELAPSED);

    glMemoryBarrier(
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
        GL_COMMAND_BARRIER_BIT
    );

    DrawArraysIndirectCommand commandData;
    glGetNamedBufferSubData(indirectCommand, 0, sizeof(DrawArraysIndirectCommand), &commandData);
    uint32 vertexCount = commandData.count;

    totalVertexCount += vertexCount;

    int32 available = 0;
    while (!available) {
        glGetQueryObjectiv(meshingQuery, GL_QUERY_RESULT_AVAILABLE, &available);
    }

    uint64 elapsedGPU;
    glGetQueryObjectui64v(meshingQuery, GL_QUERY_RESULT, &elapsedGPU);
    // dispatch time in us
    dispatchDuration = elapsedGPU / 1000;

    glDeleteQueries(1, &meshingQuery);

    if (vertexCount > 0) {
        glCreateBuffers(1, &vbo);
        glNamedBufferStorage(vbo, sizeof(Vertex) * vertexCount, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glCopyNamedBufferSubData(meshingSSBO, vbo, 0, 0, sizeof(Vertex) * vertexCount);

        glCreateVertexArrays(1, &vao);

        // position attribute in the vertex shader
        glEnableVertexArrayAttrib(vao, 0);
        glVertexArrayAttribBinding(vao, 0, 0);
        glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);

        // packed normal & color index data
        glEnableVertexArrayAttrib(vao, 1);
        glVertexArrayAttribBinding(vao, 1, 0);
        glVertexArrayAttribIFormat(vao, 1, 1, GL_UNSIGNED_INT, 3 * sizeof(GLfloat));

        glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));
    }
    else {
        vao = 0;
        vbo = 0;
    }
    
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
    glDeleteBuffers(1, &indirectCommand);
    glDeleteBuffers(1, &instanceDataBuffer);
}