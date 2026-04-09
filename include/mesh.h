#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "shader.h"

using namespace glm;

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec2 texCoords;
};

struct Texture {
	uint32 id;
	std::string type;
	std::string path;
};

class Mesh {
public:
	Mesh(std::vector<Vertex> vertices, std::vector<uint32> indices, std::vector<Texture> textures);

	void draw(Shader& shader);

	// mesh data
	std::vector<Vertex> vertices;
	std::vector<uint32> indices;
	std::vector<Texture> textures;

private:
	// render data
	uint32 VAO, VBO, EBO;

	void setupMesh();
};