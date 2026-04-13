#pragma once

#include <iostream>
#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "camera.h"
#include "camera_path.h"

#include "line.h"
#include "vox_scene.h"
#include "timer.h"

using namespace glm;

class Application {
public:
	void run();

	float lastX = 0.0f, lastY = 0.0f, deltaTime = 0.0f, lastFrame = 0.0f;
	uint32 sizeX = 0.0, sizeY = 0.0;

	bool mouseCaught = true;
	bool firstMouse = true;

	Camera cam;
	CameraPath cameraPaths[10];
	int32 activePathIdx = -1;

	VoxScene scene;

private:
	void init();
	void initWindow();
	void initOpenGL();
	void initImgui();

	void mainLoop();
	void renderImGuiFrame();

	void processInput();

	void updateCameraPath(float32 delta);

	void cleanup();

	GLFWwindow* window;
	bool enableVSync = false;
	bool enableWireframe = false;

	Line coordX, coordY, coordZ;
};