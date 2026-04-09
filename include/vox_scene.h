#pragma once

#include "shader.h"
#include "model.h"
#include "timer.h"

using namespace glm;

struct VoxScene {
	VoxScene() {};
	~VoxScene() {};

	VoxScene& operator=(VoxScene&& other) noexcept {
		return *this;
	}

	void load(const char* path);
	void render(mat4 mvp, float currentFrame);
	void cleanup();

	Model model;

	float64 total_draw_call_duration = 0.0;

	uint32 sceneVAO = 0;
	uint32 sceneVertexSSBO = 0;
	uint32 scenePackedSSBO = 0;
	uint32 sceneIBO = 0;
	uint32 sceneTransformSSBO = 0;
	uint32 sceneIndirectBuffer = 0;  // GL_DRAW_INDIRECT_BUFFER

	uint32 totalVertices = 0;
	uint32 totalIndices = 0;
	uint32 totalFaces = 0;

	Shader shader;
};