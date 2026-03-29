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

	// staging buffer size calculation based on which model is the biggest in the scene
	// can also be replaced with simple "worst case" buffer size of 128 * 128 * 128
	//int32 maxSize = 0;
	//for (size_t i = 0; i < voxScene->num_models; i++) {
	//	const ogt_vox_model* currModel = voxScene->models[i];

	//	int32 currX = (currModel->size_x + 1) >> 1;
	//	int32 currY = currModel->size_y;
	//	int32 currZ = currModel->size_z;

	//	int32 currSize = currX * currY * currZ;

	//	if (currSize > maxSize) maxSize = currSize;
	//}

	// 24 vertices per voxel max, 4 on each side
	//const uint32 maxVertices = maxSize * 6 * 4;
	//const uint32 maxIndices = maxSize * 6 * 6;
	//const uint32 maxFaces = maxSize * 6;

	//std::vector<Vertex> stagingVertices(maxVertices);
	//std::vector<uint32> stagingIndices(maxIndices);
	//std::vector<uint32> stagingPacked(maxFaces);
	//DrawElementsIndirectCommand stagingIndirect{};

	//MeshBuffers buffer;
	//buffer.vertices = stagingVertices.data();
	//buffer.indices = stagingIndices.data();
	//buffer.packedData = stagingPacked.data();
	//buffer.indirectCommand = &stagingIndirect;

	// DEBUG INFORMATION //
	uint64 totalSizeX = 0;
	uint64 totalSizeY = 0;
	uint64 totalSizeZ = 0;

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
		vec4 instanceOffset = vec4(transform.m31, transform.m32, transform.m30, 0); // swizzle offset into OpenGL coordinate space

		ivec3 rotatedModelSize;
		float64 rotationDuration = 0.0;

		float64 rotPre = local.elapsedMilliseconds();
		
		// created rotated model with CPU or GPU
		// CPU is more similar to ray marching version
		uint32 rotatedModelSSBO = 0;
		uint8* rotatedModelData = createRotatedModelCPU(voxScene, i, rotatedModelSize, rotationDuration);
		glCreateBuffers(1, &rotatedModelSSBO);
		glNamedBufferStorage(rotatedModelSSBO, rotatedModelSize.x* rotatedModelSize.y* rotatedModelSize.z, rotatedModelData, GL_DYNAMIC_STORAGE_BIT);

		rotationDurationTotal += (local.elapsedMilliseconds() - rotPre);
		rotationComputeDurationTotal += rotationDuration;

		//InstanceData instanceData;
		//instanceData.modelSize = rotatedModelSize;
		//instanceData.worldOffset = instanceOffset;

		instances.emplace_back();
		local.stop();
		forPreGenerate += local.elapsedMilliseconds();
		instances.back().generateInstanceMesh(rotatedModelData, rotatedModelSize, instanceOffset, measurements);

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
	std::cout << "  Min: " << (meshingDurationMin) << "us" << std::endl;
	std::cout << "  Max: " << (meshingDurationMax) << "us\n" << std::endl;

	std::cout << " generateMesh post dispatch duration total: " << dispatchPostTotal << "ms" << std::endl;
	std::cout << "-------------------------------" << std::endl;
	std::cout << "For loop post generateMesh: " << forPostGenerate << "ms\n" << std::endl;

	ogt_vox_destroy_scene(voxScene);

	timerTotal.stop();

	std::cout << "Scene creation total: " << timerTotal.elapsedSeconds() << " s" << std::endl;
}

void VoxScene::render(mat4 mvp, float currentFrame)
{
	glBindTextureUnit(0, palette);

	shader.use();
	shader.setInt("palette", 0);
	shader.setVec3("light_direction", -0.45f, -0.7f, -0.2f);
	//shader.setMat4("mvp", mvp);

	for (VoxInstance& instance : instances) {
		instance.render(shader, mvp);
	}
}

void VoxScene::cleanup()
{
	for (VoxInstance& instance : instances)
	{
		instance.cleanup();
	}

	glDeleteTextures(1, &palette);
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
