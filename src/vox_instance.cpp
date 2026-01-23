#include "vox_instance.h"

#include "timer.h"

void VoxInstance::generateMesh(uint32& totalVertexCount, uint32 modelSSBO, uint32 meshingSSBO, vec3 offset, ivec3 modelSize, ComputeShader& meshingX, ComputeShader& meshingY, ComputeShader& meshingZ, float64& dispatchDuration, float64& dispatchPre, float64& dispatchPost)
{
    Timer timer;
    timer.start();
    DrawArraysIndirectCommand indirectData{};
    indirectData.count = 0;
    indirectData.instanceCount = 1;
    indirectData.first = 0;
    indirectData.baseInstance = 0;

    

    glCreateBuffers(1, &indirectCommand);

    glNamedBufferStorage(indirectCommand, sizeof(DrawArraysIndirectCommand), &indirectData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    //compute shader call
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modelSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, meshingSSBO); // temp buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirectCommand);

    InstanceData instanceData{};
    instanceData.instanceDimensions = modelSize;
    instanceData.offset = offset;

    glCreateBuffers(1, &instanceDataBuffer);
    glNamedBufferStorage(instanceDataBuffer, sizeof(InstanceData), &instanceData, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, instanceDataBuffer);


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
        GL_COMMAND_BARRIER_BIT
    );
    timer.start();
    DrawArraysIndirectCommand commandData;
    glGetNamedBufferSubData(indirectCommand, 0, sizeof(DrawArraysIndirectCommand), &commandData);
    uint32 vertexCount = commandData.count;

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
    timer.stop();
    dispatchPost = timer.elapsedMilliseconds();
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