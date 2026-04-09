#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <glad/glad.h>

#include <glm/glm.hpp>

#include <cgltf/cgltf.h>

using namespace glm;

struct MeshPrimitive {
	uint32 vao;
	uint32 vbo;
	uint32 ibo;
	GLsizei indexCount;
	GLenum indexType;
	void* indexOffset;
};

struct Model {
	Model() {};
	Model(const char* path);

	void load(const char* path);
	void render();
	void cleanup();

	std::vector<MeshPrimitive> primitives;
};