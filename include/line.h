#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "shader.h"

struct Line {
	Line() {};
	Line(glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 col);
	~Line();

	Line& operator=(Line&& other) noexcept {
		if (this != &other) {
			// Clean up existing resources
			if (vbo) glDeleteBuffers(1, &vbo);
			if (vao) glDeleteVertexArrays(1, &vao);

			// Move data
			color = std::move(other.color);
			vbo = other.vbo;
			vao = other.vao;
			shader = std::move(other.shader);

			// Leave the other object in a valid state
			other.vbo = 0;
			other.vao = 0;
		}
		return *this;
	}

	void render(glm::mat4 MVP);

	glm::vec3 color;
	GLuint vbo, vao;
	Shader shader;
};