#include "vox_scene.h"

#define ROTATE_CPU

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

static inline vec4 instancePivot(const ogt_vox_model* model) {
	return floor(vec4(model->size_x / 2, model->size_y / 2, model->size_z / 2, 0.0f));
}

static inline vec3 volumeSize(const ogt_vox_model* model) {
	return vec3(model->size_x - 1, model->size_y - 1, model->size_z - 1);
}


static mat4 ogtTransformToGLM(const ogt_vox_scene* scene, const ogt_vox_instance& instance, const ogt_vox_model* model)
{
	ogt_vox_transform t = ogt_vox_sample_instance_transform(&instance, 0, scene);
	const vec4 col0(t.m00, t.m01, t.m02, t.m03);
	const vec4 col1(t.m10, t.m11, t.m12, t.m13);
	const vec4 col2(t.m20, t.m21, t.m22, t.m23);
	const vec4 col3(t.m30, t.m31, t.m32, t.m33);
	const vec3& pivot = instancePivot(model);
	return compute_transform_mat(glm::mat4(col0, col1, col2, col3), pivot);
}

void VoxScene::load(const char* path)
{
	Timer timer, timerTotal;
	timer.start();
	timerTotal.start();
	shader = Shader("../../shader/shader.vert", "../../shader/shader.frag");

	applyRotationsCompute = ComputeShader("../../shader/apply_rotations.comp");
	meshingShaders.meshingComputeX = ComputeShader("../../shader/slicing_x.comp");
	meshingShaders.meshingComputeY = ComputeShader("../../shader/slicing_y.comp");
	meshingShaders.meshingComputeZ = ComputeShader("../../shader/slicing_z.comp");

	std::cout << "Shader load done: " << timer.elapsedSeconds() << " s" << std::endl;

	const ogt_vox_scene* voxScene = load_vox_scene(path);
	if (!voxScene)
	{
		std::cerr << "Failed to load vox file at path: " << path << std::endl;
		exit(-1);
	}
	std::cout << "Scene load done: " << timer.elapsedSeconds() << " s" << std::endl;

	ogt_vox_palette ogt_palette = voxScene->palette;

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

	int64 maxQuads = (int64)maxSize * 3;

	glCreateBuffers(1, &buffers.meshingSSBO_Q);
	glNamedBufferStorage(buffers.meshingSSBO_Q, maxQuads * sizeof(Quad), nullptr, GL_DYNAMIC_STORAGE_BIT);

	instance_ranges.reserve(numInstances);

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

		ogt_vox_transform transform = ogt_vox_sample_instance_transform(currInstance, 0, voxScene);
		ivec3 instanceOffset = ivec3(transform.m31, transform.m32, transform.m30);

		ivec3 rotatedModelSize;
		float64 rotationDuration = 0.0;
		float64 rotPre = local.elapsedMilliseconds();

		uint32 rotatedModelSSBO = 0;
#ifdef ROTATE_CPU
		uint8* rotatedModelData = createRotatedModelCPU(voxScene, i, rotatedModelSize, rotationDuration);
		glCreateBuffers(1, &rotatedModelSSBO);
		glNamedBufferStorage(rotatedModelSSBO, rotatedModelSize.x * rotatedModelSize.y * rotatedModelSize.z, rotatedModelData, GL_DYNAMIC_STORAGE_BIT);
#else
		createRotatedModelBuffer(voxScene, i, rotatedModelSSBO, applyRotationsCompute, rotatedModelSize, rotationDuration);
#endif

		rotationDurationTotal += (local.elapsedMilliseconds() - rotPre);
		rotationComputeDurationTotal += rotationDuration;

		instances.emplace_back();
		local.stop();
		forPreGenerate += local.elapsedMilliseconds();

		instances.back().generateMesh(rotatedModelSSBO, buffers, meshingShaders, rotatedModelSize, instanceOffset, measurements);
#ifdef ROTATE_CPU
		instances.back().voxelData = rotatedModelData;
#endif

		VoxInstance& new_instance = instances.back();
		DrawArraysIndirectCommand cmd;
		glGetNamedBufferSubData(new_instance.indirectCommand, 0, sizeof(cmd), &cmd);

		// cmd.count is now quad count (atomic increments by 1 per quad)
		uint32 quadCount = cmd.count;

		instance_ranges.push_back({ totalQuads, quadCount, new_instance.transform });

		totalQuads += quadCount;

		local.start();
		dispatchPreTotal += measurements.dispatchPre;
		dispatchPostTotal += measurements.dispatchPost;

		totalSizeX += currModel->size_x;
		totalSizeY += currModel->size_y;
		totalSizeZ += currModel->size_z;

		float64& dispatchDuration = measurements.meshGenerationDuration;
		meshingDurationTotal += dispatchDuration;
		if (dispatchDuration < meshingDurationMin) meshingDurationMin = dispatchDuration;
		if (dispatchDuration > meshingDurationMax) meshingDurationMax = dispatchDuration;

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
	std::cout << "  Min: " << meshingDurationMin << "us" << std::endl;
	std::cout << "  Max: " << meshingDurationMax << "us\n" << std::endl;
	std::cout << " generateMesh post dispatch duration total: " << dispatchPostTotal << "ms" << std::endl;
	std::cout << "-------------------------------" << std::endl;
	std::cout << "For loop post generateMesh: " << forPostGenerate << "ms\n" << std::endl;

	timer.start();
	buildSceneBuffers(instance_ranges);
	for (VoxInstance& inst : instances)
		inst.cleanup();
	timer.stop();

	std::cout << "Scene buffer building: " << timer.elapsedMilliseconds() << "ms\n" << std::endl;

	ogt_vox_destroy_scene(voxScene);
	glDeleteBuffers(1, &buffers.meshingSSBO_Q);

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

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sceneQuadSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sceneTransformSSBO);

	glBindVertexArray(sceneVAO);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sceneIndirectBuffer);

	glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, numDrawCmds, 0);

	glBindVertexArray(0);
}

void VoxScene::cleanup()
{
	glDeleteVertexArrays(1, &sceneVAO);
	glDeleteBuffers(1, &sceneQuadSSBO);
	glDeleteBuffers(1, &sceneTransformSSBO);
	glDeleteBuffers(1, &sceneIndirectBuffer);
	glDeleteTextures(1, &palette);
}

void VoxScene::createRotatedModelBuffer(const ogt_vox_scene* scene, uint32 instanceIdx, uint32& rotatedModelSSBO, ComputeShader& compute, ivec3& rotatedModelSize, float64& dispatchDuration)
{
	const ogt_vox_instance& instance = scene->instances[instanceIdx];
	const ogt_vox_model* model = scene->models[instance.model_index];
	mat4& transformMat = ogtTransformToGLM(scene, instance, model);

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
	rotatedModelSize = ivec3(rotatedModelSize.y, rotatedModelSize.z, rotatedModelSize.x); // swizzle size into correct coordinate space

	// apply_rotations_compute
	const uint8* voxelData = model->voxel_data;

	uint32 instanceTempSSBO;
	glCreateBuffers(1, &instanceTempSSBO);
	glCreateBuffers(1, &rotatedModelSSBO);

	glNamedBufferStorage(instanceTempSSBO, sizeof(uint8_t) * model->size_x * model->size_y * model->size_z, voxelData, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferStorage(rotatedModelSSBO, sizeof(uint8_t) * model->size_x * model->size_y * model->size_z, nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);
	glClearNamedBufferData(rotatedModelSSBO, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); // all values are initially 0. 0 = empty voxel

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, instanceTempSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, rotatedModelSSBO);

	RotationData rotationData{};
	rotationData.instanceSize = vec4(model->size_x, model->size_y, model->size_z, 1.0);
	rotationData.rotatedSize = vec4(rotatedModelSize, 1.0);
	rotationData.minBounds = vec4(minBounds, 1.0);
	rotationData.transform = transformMat;

	uint32 rotationDataUBO;
	glCreateBuffers(1, &rotationDataUBO);

	glNamedBufferStorage(rotationDataUBO, sizeof(RotationData), &rotationData, GL_DYNAMIC_STORAGE_BIT);

	glBindBufferBase(GL_UNIFORM_BUFFER, 2, rotationDataUBO);

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
	glDeleteBuffers(1, &rotationDataUBO);
}

uint8* VoxScene::createRotatedModelCPU(const ogt_vox_scene* scene, uint32 instanceIdx, ivec3& rotatedModelSize, float64& dispatchDuration)
{
	const ogt_vox_instance& instance = scene->instances[instanceIdx];
	const ogt_vox_model* model = scene->models[instance.model_index];
	mat4 transformMat = ogtTransformToGLM(scene, instance, model);

	vec3 corners[8] = {
		{0, 0, 0}, {model->size_x - 1, 0, 0}, 
		{0, model->size_y - 1, 0}, 
		{0, 0, model->size_z - 1},
		{model->size_x - 1, model->size_y - 1, 0}, 
		{model->size_x - 1, 0, model->size_z - 1},
		{0, model->size_y - 1, model->size_z - 1}, 
		{model->size_x - 1, model->size_y - 1, model->size_z - 1}
	};

	vec3 minBounds(FLT_MAX);
	vec3 maxBounds(-FLT_MAX);

	for (int i = 0; i < 8; ++i) {
		vec4 transformedCorner = transformMat * vec4(corners[i], 1.0f);
		vec3 flooredCorner = floor(vec3(transformedCorner));
		minBounds = min(minBounds, flooredCorner);
		maxBounds = max(maxBounds, flooredCorner);
	}

	rotatedModelSize = ivec3(maxBounds - minBounds) + ivec3(1);
	rotatedModelSize = ivec3(rotatedModelSize.y, rotatedModelSize.z, rotatedModelSize.x);

	size_t numVoxels = (size_t)rotatedModelSize.x * rotatedModelSize.y * rotatedModelSize.z;
	uint8* outData = (uint8*)calloc(numVoxels, sizeof(uint8));

	uint32 sizeX = model->size_x;
	uint32 sizeY = model->size_y;
	uint32 sizeZ = model->size_z;

#pragma omp parallel for collapse(2) schedule(static)
	for (int32 z = 0; z < sizeZ; ++z) {
		for (int32 y = 0; y < sizeY; ++y) {
			for (int32 x = 0; x < sizeX; ++x) {

				uint32 srcIdx = x + (y * sizeX) + (z * sizeX * sizeY);
				uint8 colIdx = model->voxel_data[srcIdx];

				if (colIdx == 0) continue;

				// Apply transform
				vec4 rotatedPos = floor(transformMat * vec4((float32)x, (float32)y, (float32)z, 1.0f));
				ivec3 finalPos = ivec3(vec3(rotatedPos.x, rotatedPos.y, rotatedPos.z) - minBounds);
				finalPos = ivec3(finalPos.y, finalPos.z, finalPos.x);

				// Bounds check
				if (finalPos.x >= 0 && finalPos.x < rotatedModelSize.x &&
					finalPos.y >= 0 && finalPos.y < rotatedModelSize.y &&
					finalPos.z >= 0 && finalPos.z < rotatedModelSize.z)
				{
					uint32 dstIdx = finalPos.x + (finalPos.y * rotatedModelSize.x) + (finalPos.z * rotatedModelSize.x * rotatedModelSize.y);
					outData[dstIdx] = colIdx;
				}
			}
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();

	return outData;
}

void VoxScene::buildSceneBuffers(const std::vector<InstanceRange>& ranges)
{
	glCreateBuffers(1, &sceneQuadSSBO);
	glNamedBufferStorage(sceneQuadSSBO, sizeof(Quad) * totalQuads, nullptr, GL_DYNAMIC_STORAGE_BIT);

	// DrawArraysIndirectCommand: count, instanceCount, first, baseInstance
	struct DrawArraysIndirectCommand {
		uint32 count;         // quadCount * 6
		uint32 instanceCount;
		uint32 first;         // quadOffset * 6
		uint32 baseInstance;  // indexes into transforms[]
	};

	std::vector<DrawArraysIndirectCommand> cmds;
	std::vector<mat4> transforms;
	cmds.reserve(instances.size());
	transforms.reserve(instances.size());

	for (size_t i = 0; i < instances.size(); i++)
	{
		VoxInstance& curr_instance = instances[i];
		const InstanceRange& curr_range = ranges[i];

		if (curr_range.quadCount == 0) continue;

		glCopyNamedBufferSubData(
			curr_instance.quadSSBO, sceneQuadSSBO,
			0,
			sizeof(Quad) * curr_range.quadOffset,
			sizeof(Quad) * curr_range.quadCount
		);

		DrawArraysIndirectCommand cmd{};
		cmd.count = curr_range.quadCount * 6;
		cmd.instanceCount = 1;
		cmd.first = curr_range.quadOffset * 6;
		cmd.baseInstance = (uint32)transforms.size();
		cmds.push_back(cmd);
		transforms.push_back(curr_range.transform);
	}

	numDrawCmds = (uint32)cmds.size();

	glCreateBuffers(1, &sceneIndirectBuffer);
	glNamedBufferStorage(sceneIndirectBuffer,
		sizeof(DrawArraysIndirectCommand) * cmds.size(),
		cmds.data(), GL_DYNAMIC_STORAGE_BIT);

	glCreateBuffers(1, &sceneTransformSSBO);
	glNamedBufferStorage(sceneTransformSSBO,
		sizeof(mat4) * transforms.size(),
		transforms.data(), GL_DYNAMIC_STORAGE_BIT);

	// Empty VAO — no vertex attributes, all data pulled from SSBOs
	glCreateVertexArrays(1, &sceneVAO);
}
