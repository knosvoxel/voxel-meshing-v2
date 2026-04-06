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

void VoxScene::buildSceneBuffers()
{
	for (VoxInstance& instance : instances)
	{
		int32 offset = (int32)sceneQuads.size();
		for (int32 f : instance.firstVertices)
		{
			firstVerticesPerChunk.push_back((f + offset) * 6);
		}

		sceneVertexCounts.insert(sceneVertexCounts.end(), instance.vertexCounts.begin(), instance.vertexCounts.end());
		sceneTransforms.insert(sceneTransforms.end(), instance.transforms.begin(), instance.transforms.end());
		sceneQuads.insert(sceneQuads.end(), instance.instanceQuads.begin(), instance.instanceQuads.end());
	}

	glCreateBuffers(1, &sceneVertexSSBO);
	glNamedBufferStorage(sceneVertexSSBO,
		sizeof(uint64) * sceneQuads.size(),
		sceneQuads.data(), 0);

	glCreateBuffers(1, &sceneTransformSSBO);
	glNamedBufferStorage(sceneTransformSSBO,
		sizeof(mat4) * sceneTransforms.size(),
		sceneTransforms.data(), 0);

	glCreateVertexArrays(1, &sceneVAO);
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

	std::cout << "Meshing instance... " << std::endl;

	// DEBUG INFORMATION //
	uint64 totalSizeX = 0;
	uint64 totalSizeY = 0;
	uint64 totalSizeZ = 0;

	float64 meshingDurationTotal = 0.0;
	float64 meshingDurationMin = DBL_MAX;
	float64 meshingDurationMax = 0.0;

	float64 meshPreTotal = 0.0;
	float64 meshInstanceChunksTotal = 0.0;
	float64 meshPostTotal = 0.0;

	float64 forPreGenerate = 0.0;

	float64 rotationDurationTotal = 0.0;

	// Chunks
	uint32 chunk_count = 0;
	float64 chunkTotalOccupancyMask = 0.0;
	float64 chunkTotalFaceCulling = 0.0;
	float64 chunkTotalMeshing = 0.0;

	////////////////////////
	timer.start();


	for (size_t i = 0; i < numInstances; i++)
	{
		std::cout << i << " ";
		
		measurements = MeasurementData{};

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
		
		// created rotated model with CPU
		uint8* rotatedModelData = createRotatedModelCPU(voxScene, i, rotatedModelSize, rotationDuration);

		rotationDurationTotal += (local.elapsedMilliseconds() - rotPre);

		instances.emplace_back();
		local.stop();
		forPreGenerate += local.elapsedMilliseconds();
		instances.back().generateInstanceMesh(rotatedModelData, rotatedModelSize, instanceOffset, measurements);

		meshPreTotal += measurements.meshPre;
		meshInstanceChunksTotal += measurements.meshInstanceChunks;
		meshPostTotal += measurements.meshPost;
		meshingDurationTotal += measurements.meshTotal;
		if (measurements.meshTotal < meshingDurationMin) meshingDurationMin = measurements.meshTotal;
		if (measurements.meshTotal > meshingDurationMax) meshingDurationMax = measurements.meshTotal;

		chunk_count += measurements.actuallyMeshedChunkCount;
		total_vertices += measurements.vertexCount;

		chunkTotalOccupancyMask += measurements.chunkMeasurements.occupancyMaskTotal;
		chunkTotalFaceCulling += measurements.chunkMeasurements.faceCullingTotal;
		chunkTotalMeshing += measurements.chunkMeasurements.meshingTotal;

		totalSizeX += currModel->size_x;
		totalSizeY += currModel->size_y;
		totalSizeZ += currModel->size_z;
	}

	uint64 curr = timer.elapsedMilliseconds();

	buildSceneBuffers();

	timer.stop();

	std::cout << "\n\nAmount of chunks: " << chunk_count << std::endl;

	std::cout << "Average instance size: " << totalSizeX / numInstances << " " << totalSizeY / numInstances << " " << totalSizeZ / numInstances << std::endl;
	std::cout << "Average amount of chunks per instance: " << chunk_count / numInstances << "\n" << std::endl;

	std::cout << "Meshing Loop Duration total: " << timer.elapsedMilliseconds() << "ms\n" << std::endl;


	std::cout << "For loop pre generateMesh: " << forPreGenerate << "ms" << std::endl;
	std::cout << " Rotation duration total: " << rotationDurationTotal << "ms" << std::endl;

	std::cout << "---------- generateMesh -------" << std::endl;
	double mean = meshingDurationTotal / numInstances;
	std::cout << " Meshing duration: " << std::endl;
	std::cout << "  Total: " << meshingDurationTotal << "ms (" << meshingDurationTotal / 1000.0 << "s)" << std::endl;
	std::cout << "  Average per instance: " << mean << "ms" << std::endl;
	std::cout << "  Average per chunk: " << meshingDurationTotal / chunk_count << "ms" << std::endl;
	std::cout << "  Min: " << (meshingDurationMin) << "ms" << std::endl;
	std::cout << "  Max: " << (meshingDurationMax) << "ms\n" << std::endl;

	std::cout << "In Detail: " << std::endl;
	std::cout << " chunk generation duration total: " << meshPreTotal << "ms" << std::endl;
	std::cout << " average chunk generation duration: " << meshPreTotal / numInstances << "ms\n" << std::endl;
	
	std::cout << " meshing per instance total: " << meshInstanceChunksTotal << "ms" << std::endl;
	std::cout << " meshing per instance average: " << meshInstanceChunksTotal / numInstances << "ms" << std::endl;
	std::cout << " Per Chunks: " << std::endl;
	std::cout << "  Occupancy mask generation total (Combined across all threads, not actual time): " << chunkTotalOccupancyMask << "ms" << std::endl;
	std::cout << "  Occupancy mask generation average per chunk: " << chunkTotalOccupancyMask / chunk_count << "ms" << std::endl;
	std::cout << "  Face culling total (Combined across all threads, not actual time): " << chunkTotalFaceCulling << "ms" << std::endl;
	std::cout << "  Face culling average per chunk (Combined across all threads, not actual time): " << chunkTotalFaceCulling / chunk_count << "ms" << std::endl;
	std::cout << "  Meshing from mask total: " << chunkTotalMeshing << "ms" << std::endl;
	std::cout << "  Meshing from mask average per chunk: " << chunkTotalMeshing / chunk_count << "ms" << std::endl;
	std::cout << std::endl;

	std::cout << " instance buffer creation total: " << meshPostTotal << "ms" << std::endl;
	std::cout << " instance buffer creation average: " << meshPostTotal / numInstances << "ms" << std::endl;
	std::cout << "-------------------------------" << std::endl;

	std::cout << "Combine instance buffers into scene buffer: " << timer.elapsedMilliseconds() - curr << "ms\n" << std::endl;

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
	shader.setMat4("mvp", mvp);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sceneVertexSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sceneTransformSSBO);
	glBindVertexArray(sceneVAO);

	glMultiDrawArrays(GL_TRIANGLES,
		firstVerticesPerChunk.data(),
		sceneVertexCounts.data(),
		(GLsizei)firstVerticesPerChunk.size());

	glBindVertexArray(0);
}

void VoxScene::cleanup()
{
	glDeleteVertexArrays(1, &sceneVAO);
	glDeleteBuffers(1, &sceneVertexSSBO);
	glDeleteBuffers(1, &sceneTransformSSBO);
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
					uint32 dstIdx = finalPos.y
						+ (finalPos.x * rotatedModelSize.y)
						+ (finalPos.z * rotatedModelSize.y * rotatedModelSize.x);
					outData[dstIdx] = colIdx;
				}
			}
		}
	}

	auto endTime = std::chrono::high_resolution_clock::now();

	return outData;
}
