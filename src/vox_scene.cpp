#include "vox_scene.h"

static inline mat4 compute_transform_mat(const mat4 transform, const vec3& pivot) {
	static const mat4 shift_matrix = translate(mat4(1.0f), vec3(0.5f));
	const mat4 pivot_matrix = translate(mat4(1.0f), -pivot);
	const mat4 combined_matrix = transform * shift_matrix * pivot_matrix;
	return combined_matrix;
}

inline ivec3 calc_transform(const mat4& mat, const vec3& pos)
{
	return floor(mat * vec4(pos, 1.0f));
}

static inline vec4 instance_pivot(const ogt_vox_model* model) {
	return floor(vec4(model->size_x / 2, model->size_y / 2, model->size_z / 2, 0.0f));
}

static inline vec3 volume_size(const ogt_vox_model* model) {
	return vec3(model->size_x - 1, model->size_y - 1, model->size_z - 1);
}


static mat4 ogt_transform_to_glm(const ogt_vox_scene* scene, const ogt_vox_instance& instance, const ogt_vox_model* model)
{
	ogt_vox_transform t = ogt_vox_sample_instance_transform(&instance, 0, scene);
	const vec4 col0(t.m00, t.m01, t.m02, t.m03);
	const vec4 col1(t.m10, t.m11, t.m12, t.m13);
	const vec4 col2(t.m20, t.m21, t.m22, t.m23);
	const vec4 col3(t.m30, t.m31, t.m32, t.m33);
	const vec3& pivot = instance_pivot(model);
	return compute_transform_mat(glm::mat4(col0, col1, col2, col3), pivot);
}

void VoxScene::load(const char* path)
{
	Timer timer, timerTotal;
	timer.start();
	timerTotal.start();
	shader = Shader("../../shader/shader.vert", "../../shader/shader.frag");

	applyRotationsCompute = ComputeShader("../../shader/apply_rotations.comp");
	meshingCompute = ComputeShader("../../shader/slicing_v2.comp");

	std::cout << "Shader load done: " << timer.elapsedSeconds() << " s" << std::endl;

	const ogt_vox_scene* voxScene = load_vox_scene(path);
	if (!voxScene) 
	{
		std::cerr << "Failed to load vox file at path: " << path << std::endl;
		exit(-1);
	}
	std::cout << "Scene load done: " << timer.elapsedSeconds() << " s" << std::endl;

	ogt_vox_palette ogt_palette = voxScene->palette;

	// texture generation with DSA
	glCreateTextures(GL_TEXTURE_2D, 1, &palette);

	glTextureParameteri(palette, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(palette, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(palette, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(palette, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureStorage2D(palette, 1, GL_RGBA8, 256, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(palette, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, ogt_palette.color);
	glBindImageTexture(0, palette, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

	numInstances = voxScene->num_instances;
	instances.reserve(numInstances);

	timer.stop();
	std::cout << "Scene Shader & palette overhead total: " << timer.elapsedSeconds() << " s\n" << std::endl;

	std::cout << numInstances << " instance(s)\n" << std::endl;
	auto meshingSSBOStart = timerTotal.elapsedMilliseconds();

	// staging buffer size calculation based on which model is the biggest in the scene
	// can also be replaced with simple "worst case" buffer size of 128 * 128 * 128
	int32 maxSize = 0;
	for (size_t i = 0; i < voxScene->num_models; i++) {
		const ogt_vox_model* currModel = voxScene->models[i];

		int32 currX = (currModel->size_x + 1) >> 1;
		int32 currY = currModel->size_y;
		int32 currZ = currModel->size_z;

		int32 currSize = currX * currY * currZ;

		if (currSize > maxSize) maxSize = currSize;
	}

	auto meshingSSBOEnd = timerTotal.elapsedMilliseconds();
	std::cout << "meshingSSBO size calculation: " << meshingSSBOEnd - meshingSSBOStart << " ms\n" << std::endl;

	// create temporary worst case buffer
	uint32 meshingSSBO;
	glCreateBuffers(1, &meshingSSBO);
	glNamedBufferStorage(meshingSSBO, maxSize * sizeof(Vertex) * 36,
		nullptr, GL_DYNAMIC_STORAGE_BIT);

	// DEBUG INFORMATION //
	uint64_t totalSizeX = 0;
	uint64_t totalSizeY = 0;
	uint64_t totalSizeZ = 0;

	float64 dispatchPreTotal = 0.0;
	float64 dispatchPostTotal = 0.0;
	float64 meshingDurationTotal = 0.0;
	float64 meshingDurationMin = DBL_MAX;
	float64 meshingDurationMax = 0.0;

	float64 forPreGenerate = 0.0;
	float64 forPostGenerate = 0.0;

	float64 rotationComputeDurationTotal = 0.0;
	float64 rotationDurationTotal = 0.0;
	////////////////////////
	timer.start();
	for (size_t i = 0; i < numInstances; i++)
	{
		Timer local;
		local.start();
		const ogt_vox_instance* currInstance = &voxScene->instances[i];
		const ogt_vox_model* currModel = voxScene->models[currInstance->model_index];

		//voxelCount += count_solid_voxels_in_model(currModel);

		ogt_vox_transform transform = ogt_vox_sample_instance_transform(currInstance, 0, voxScene);
		vec4 instanceOffset = vec4(transform.m30, transform.m31, transform.m32, 0);

		ivec3 rotatedModelSize;
		float64 rotationDuration = 0.0;

		float64 rotPre = local.elapsedMilliseconds();
		uint32 rotatedModelBuffer = createRotatedModelBuffer(voxScene, i, applyRotationsCompute, rotatedModelSize, rotationDuration);
		rotationDurationTotal += (local.elapsedMilliseconds() - rotPre);
		rotationComputeDurationTotal += rotationDuration;

		float64 meshGenerationDuration = 0.0;
		float64 dispatchPre = 0.0;
		float64 dispatchPost = 0.0;
		
		instances.emplace_back();
		local.stop();
		forPreGenerate += local.elapsedMilliseconds();
		instances.back().generateMesh(vertexCount, rotatedModelBuffer, meshingSSBO, instanceOffset, rotatedModelSize, meshingCompute, meshGenerationDuration, dispatchPre, dispatchPost);
		local.start();
		dispatchPreTotal += dispatchPre;
		dispatchPostTotal += dispatchPost;

		glDeleteBuffers(1, &rotatedModelBuffer);

		totalSizeX += currModel->size_x;
		totalSizeY += currModel->size_y;
		totalSizeZ += currModel->size_z;

		meshingDurationTotal += meshGenerationDuration;
		if (meshGenerationDuration < meshingDurationMin) meshingDurationMin = meshGenerationDuration;
		if (meshGenerationDuration > meshingDurationMax) meshingDurationMax = meshGenerationDuration;
		local.stop();
		forPostGenerate += local.elapsedMilliseconds();
	}
	timer.stop();

	std::cout << "Average instance size: " << totalSizeX / numInstances << " " << totalSizeY / numInstances << " " << totalSizeZ / numInstances << "\n" << std::endl;

	std::cout << "Meshing Loop Duration total: " << timer.elapsedMilliseconds() << "ms\n" << std::endl;


	std::cout << "For loop pre generateMesh: " << forPreGenerate << "ms" << std::endl;
	std::cout << " Rotation duration total: " << rotationDurationTotal << "ms" << std::endl;
	std::cout << " Rotation compute duration total: " << rotationComputeDurationTotal / 1000.0 << "ms" << std::endl;

	std::cout << "---------- generateMesh -------" << std::endl;
	std::cout << " generateMesh pre  dispatch duration total: " << dispatchPreTotal << "ms\n" << std::endl;

	double mean = meshingDurationTotal / numInstances;
	std::cout << " Meshing Compute duration: " << std::endl;
	std::cout << "  Total: " << meshingDurationTotal << "us (" << meshingDurationTotal / 1000.0 << "ms)" << std::endl;
	std::cout << "  Average: " << mean << "us" << std::endl;
	std::cout << "  Min: " << (meshingDurationMin) << "us" << std::endl;
	std::cout << "  Max: " << (meshingDurationMax) << "us\n" << std::endl;

	std::cout << " generateMesh post dispatch duration total: " << dispatchPostTotal << "ms" << std::endl;
	std::cout << "-------------------------------" << std::endl;
	std::cout << "For loop post generateMesh: " << forPostGenerate << "ms\n" << std::endl;

	ogt_vox_destroy_scene(voxScene);

	glDeleteBuffers(1, &meshingSSBO);

	timerTotal.stop();

	std::cout << "Scene creation total: " << timerTotal.elapsedSeconds() << " s" << std::endl;
}

void VoxScene::render(mat4 mvp, float currentFrame)
{
	glBindTextureUnit(0, palette);

	shader.use();
	shader.setInt("palette", 0);
	shader.setVec3("light_direction", -0.45f, -0.7f, -0.2f);
	shader.setMat4("mvp", mvp);

	for (VoxInstance& instance : instances) {
		instance.render();
	}
}

void VoxScene::cleanup()
{
	for (VoxInstance instance : instances)
	{
		instance.cleanup();
	}

	glDeleteTextures(1, &palette);
}

uint32 VoxScene::createRotatedModelBuffer(const ogt_vox_scene* scene, uint32 instanceIdx, ComputeShader& compute, ivec3& rotatedModelSize, float64& dispatchDuration)
{
	const ogt_vox_instance& instance = scene->instances[instanceIdx];
	const ogt_vox_model* model = scene->models[instance.model_index];
	mat4& transformMat = ogt_transform_to_glm(scene, instance, model);

	vec3 corners[8] = {
		{0, 0, 0},
		{model->size_x - 1, 0, 0},
		{0, model->size_y - 1, 0},
		{0, 0, model->size_z - 1},
		{model->size_x - 1, model->size_y - 1, 0},
		{model->size_x - 1, 0, model->size_z - 1},
		{0, model->size_y - 1, model->size_z - 1},
		{model->size_x - 1, model->size_y - 1, model->size_z - 1},
	};

	// transform each corner of bouding box individually
	vec3 minBounds(FLT_MAX);
	vec3 maxBounds(-FLT_MAX);

	for (int i = 0; i < 8; ++i) {
		vec4 transformedCorner = transformMat * vec4(corners[i], 1.0f);
		vec3 flooredCorner = floor(vec3(transformedCorner));
		minBounds = min(minBounds, flooredCorner);
		maxBounds = max(maxBounds, flooredCorner);
	}

	rotatedModelSize = ivec3(maxBounds - minBounds) + ivec3(1); // +1 since voxel grids are inclusive

	// apply_rotations_compute
	const uint8* voxelData = model->voxel_data;

	uint32 instanceTempSSBO, rotatedModelSSBO;
	glCreateBuffers(1, &instanceTempSSBO);
	glCreateBuffers(1, &rotatedModelSSBO);

	glNamedBufferStorage(instanceTempSSBO, sizeof(uint8_t) * model->size_x * model->size_y * model->size_z, voxelData, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferStorage(rotatedModelSSBO, sizeof(uint8_t) * model->size_x * model->size_y * model->size_z, nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
	glClearNamedBufferData(rotatedModelSSBO, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); // all values are initially 0. 0 = empty voxel

	RotationData rotationData{};
	rotationData.instanceSize = vec4(model->size_x, model->size_y, model->size_z, 1.0);
	rotationData.rotatedSize = vec4(rotatedModelSize, 1.0);
	rotationData.minBounds = vec4(minBounds, 1.0);
	rotationData.transform = transformMat;

	uint32 rotationDataTempBuffer;
	glCreateBuffers(1, &rotationDataTempBuffer);

	glNamedBufferStorage(rotationDataTempBuffer, sizeof(RotationData), &rotationData, GL_DYNAMIC_STORAGE_BIT);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, instanceTempSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, rotatedModelSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rotationDataTempBuffer);

	compute.use();

	uint32 rotationQuery;
	glGenQueries(1, &rotationQuery);
	glBeginQuery(GL_TIME_ELAPSED, rotationQuery);

	uint32 dispatchSizeX = (model->size_x + 15) / 16;
	uint32 dispatchSizeY = (model->size_y + 15) / 16;

	// apply_rotations_compute
	glDispatchCompute(dispatchSizeX, dispatchSizeY, model->size_z);

	glEndQuery(GL_TIME_ELAPSED);

	glMemoryBarrier(
		GL_SHADER_STORAGE_BARRIER_BIT
	);

	int32 available = 0;
	while (!available) {
		glGetQueryObjectiv(rotationQuery, GL_QUERY_RESULT_AVAILABLE, &available);
	}

	uint64 elapsedGPU;
	glGetQueryObjectui64v(rotationQuery, GL_QUERY_RESULT, &elapsedGPU);
	// dispatch time in us
	dispatchDuration = elapsedGPU / 1000;

	glDeleteQueries(1, &rotationQuery);

	glDeleteBuffers(1, &instanceTempSSBO);
	glDeleteBuffers(1, &rotationDataTempBuffer);

	return rotatedModelSSBO;
}
