#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cgltf/cgltf.h>

#include <stb_image/stb_image.h>

#include "shader.h"

using namespace glm;

struct MeshPrimitive {
    uint32 vao = 0;
    std::vector<uint32> vbos;
    uint32 ibo = 0;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_INT;
    void* indexOffset = nullptr;
    mat4 nodeTransform = mat4(1.0f);
    int materialIndex = -1;
    uint32 textureID = 0;
};

struct Model {
	Model() {};
	Model(const char* path);

	void load(const char* path);
	void render(Shader& shader, mat4 mvp);
	void cleanup();

    std::vector<MeshPrimitive> primitives;
    std::vector<uint32> textureIDs;
};