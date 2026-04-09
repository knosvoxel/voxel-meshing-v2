#include "model.h"

Model::Model(const char* path)
{
	load(path);
}

void Model::load(const char* path)
{
	cgltf_options options = {};
	cgltf_data* data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, path, &data);

	if (result != cgltf_result_success)
	{
		throw std::runtime_error("Failed to parse glTF");
	}

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success)
	{
		throw std::runtime_error("Failed to load glTF buffers");
	}

	for (cgltf_size i = 0; i < data->meshes_count; ++i)
		for (cgltf_size j = 0; j < data->meshes[i].primitives_count; ++j)
		{
			cgltf_primitive& prim = data->meshes[i].primitives[j];
			
			MeshPrimitive outPrim = {};

			glCreateVertexArrays(1, &outPrim.vao);

			for (cgltf_size k = 0; k < prim.attributes_count; ++k)
			{
				cgltf_attribute& attr = prim.attributes[k];
				cgltf_accessor* accessor = attr.data;
				cgltf_buffer_view* view = accessor->buffer_view;

				uint32 vbo;
				glCreateBuffers(1, &vbo);
				outPrim.vbos.push_back(vbo);

				uint8* bufferData = (uint8*)view->buffer->data + view->offset;

				glNamedBufferStorage(vbo, view->size, bufferData, 0);

				uint32 loc = 0;
				if (attr.type == cgltf_attribute_type_position) loc = 0;
				else if (attr.type == cgltf_attribute_type_normal) loc = 1;
				else if (attr.type == cgltf_attribute_type_texcoord) loc = 2;
				else continue;

				glEnableVertexArrayAttrib(outPrim.vao, loc);

				GLenum glType;
				switch (accessor->component_type) {
				case cgltf_component_type_r_8:   glType = GL_BYTE; break;
				case cgltf_component_type_r_8u:  glType = GL_UNSIGNED_BYTE; break;
				case cgltf_component_type_r_16:  glType = GL_SHORT; break;
				case cgltf_component_type_r_16u: glType = GL_UNSIGNED_SHORT; break;
				case cgltf_component_type_r_32u: glType = GL_UNSIGNED_INT; break;
				case cgltf_component_type_r_32f: glType = GL_FLOAT; break;
				default: glType = GL_FLOAT; break;
				}
				GLboolean normalized = accessor->normalized ? GL_TRUE : GL_FALSE;
				glVertexArrayAttribFormat(outPrim.vao, loc, (GLint)cgltf_num_components(accessor->type), glType, normalized, 0);
				glVertexArrayAttribBinding(outPrim.vao, loc, loc);
				glVertexArrayVertexBuffer(outPrim.vao, loc, vbo, (GLintptr)accessor->offset, (GLsizei)(view->stride ? view->stride : accessor->stride));
			}

			if (prim.indices) {
				cgltf_accessor* accessor = prim.indices;
				cgltf_buffer_view* view = accessor->buffer_view;

				glCreateBuffers(1, &outPrim.ibo);
				uint8* bufferData = (uint8*)view->buffer->data + view->offset;
				glNamedBufferStorage(outPrim.ibo, view->size, bufferData, 0);

				glVertexArrayElementBuffer(outPrim.vao, outPrim.ibo);

				outPrim.indexCount = (GLsizei)accessor->count;
				outPrim.indexType = (accessor->component_type == cgltf_component_type_r_16u) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
				outPrim.indexOffset = (void*)accessor->offset;
			}

			if (prim.material) {
				outPrim.materialIndex = (int)(prim.material - data->materials);
			}

			primitives.push_back(outPrim);
		}

	for (cgltf_size i = 0; i < data->nodes_count; ++i) {
		cgltf_node& node = data->nodes[i];
		if (!node.mesh) continue;

		mat4 transform = mat4(1.0f);
		float mtx[16];
		cgltf_node_transform_local(&node, mtx);
		transform = glm::make_mat4(mtx);

		for (cgltf_size j = 0; j < node.mesh->primitives_count; ++j) {
			size_t primIndex = (node.mesh - data->meshes) * node.mesh->primitives_count + j;
			if (primIndex < primitives.size()) {
				primitives[primIndex].nodeTransform = transform;
			}
		}
	}

	// Load textures
	for (cgltf_size i = 0; i < data->materials_count; ++i) {
		cgltf_material& mat = data->materials[i];
		if (!mat.has_pbr_metallic_roughness) continue;

		cgltf_texture_view& texView = mat.pbr_metallic_roughness.base_color_texture;
		if (!texView.texture || !texView.texture->image) continue;

		cgltf_image* img = texView.texture->image;

		int width, height, channels;
		unsigned char* imgData = nullptr;

		if (img->buffer_view) {
			uint8* rawData = (uint8*)img->buffer_view->buffer->data + img->buffer_view->offset;
			size_t rawSize = img->buffer_view->size;
			imgData = stbi_load_from_memory(rawData, (int)rawSize, &width, &height, &channels, 4);
		}
		else if (img->uri) {
			std::string texPath = std::string(path);
			size_t lastSlash = texPath.find_last_of("/\\");
			if (lastSlash != std::string::npos)
				texPath = texPath.substr(0, lastSlash + 1) + img->uri;
			else
				texPath = img->uri;
			imgData = stbi_load(texPath.c_str(), &width, &height, &channels, 4);
		}

		if (!imgData) {
			std::cerr << "Failed to load texture" << std::endl;
			continue;
		}

		uint32 texID;
		glCreateTextures(GL_TEXTURE_2D, 1, &texID);
		glTextureStorage2D(texID, 1, GL_RGBA8, width, height);
		glTextureSubImage2D(texID, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, imgData);
		glGenerateTextureMipmap(texID);
		glTextureParameteri(texID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(texID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(texID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(texID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		stbi_image_free(imgData);
		textureIDs.push_back(texID);

		for (auto& prim : primitives) {
			if (prim.materialIndex == (int)i) {
				prim.textureID = texID;
			}
		}
	}

	cgltf_free(data);
}

void Model::render(Shader& shader, mat4 mvp)
{
	for (const auto& prim : primitives)
	{
		shader.setMat4("mvp", mvp * prim.nodeTransform);

		if (prim.textureID != 0) {
			glBindTextureUnit(0, prim.textureID);
		}

		glBindVertexArray(prim.vao);
		if (prim.ibo != 0) {
			glDrawElements(GL_TRIANGLES, prim.indexCount, prim.indexType, prim.indexOffset);
		}
		else {
			glDrawArrays(GL_TRIANGLES, 0, prim.indexCount);
		}
	}
	glBindVertexArray(0);
}

void Model::cleanup() {
	for (auto& prim : primitives) {
		glDeleteVertexArrays(1, &prim.vao);
		glDeleteBuffers((GLsizei)prim.vbos.size(), prim.vbos.data());
		if (prim.ibo) glDeleteBuffers(1, &prim.ibo);
	}
	if (!textureIDs.empty())
		glDeleteTextures((GLsizei)textureIDs.size(), textureIDs.data());
}