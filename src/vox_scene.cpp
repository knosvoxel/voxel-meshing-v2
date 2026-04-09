#include "vox_scene.h"

void VoxScene::load(const char* path)
{
	Timer timer, timerTotal;
	timer.start();
	timerTotal.start();
	shader = Shader("../../shader/shader.vert", "../../shader/shader.frag");

	std::cout << "Shader load done: " << timer.elapsedSeconds() << " s" << std::endl;

	model = Model(path);

	timer.stop();
	std::cout << "Scene Shader & palette overhead total: " << timer.elapsedSeconds() << " s\n" << std::endl;

	auto meshingSSBOStart = timerTotal.elapsedMilliseconds();


	std::cout << "Scene creation total: " << timerTotal.elapsedSeconds() << " s" << std::endl;
}

void VoxScene::render(mat4 mvp, float currentFrame)
{
	shader.use();
	shader.setVec3("light_direction", normalize(vec3(-0.45f, -0.7f, -0.2f)));
	shader.setMat4("mvp", mvp);

	model.render();

	glBindVertexArray(0);
}

void VoxScene::cleanup()
{
	model.cleanup();
}