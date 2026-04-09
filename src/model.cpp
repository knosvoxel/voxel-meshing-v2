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

				uint8* bufferData = (uint8*)view->buffer->data + view->offset;

				glNamedBufferStorage(vbo, view->size, bufferData, 0);

				uint32 loc = 0;
				if (attr.type == cgltf_attribute_type_position) loc = 0;
				else if (attr.type == cgltf_attribute_type_normal) loc = 1;
				else if (attr.type == cgltf_attribute_type_texcoord) loc = 2;
				else continue;

				glEnableVertexArrayAttrib(outPrim.vao, loc);
				glVertexArrayAttribFormat(outPrim.vao, loc, (int32)cgltf_num_components(accessor->type), GL_FLOAT, GL_FALSE, 0);
				glVertexArrayAttribBinding(outPrim.vao, loc, loc);
				glVertexArrayVertexBuffer(outPrim.vao, loc, vbo, (int32)accessor->offset, (GLsizei)view->stride ? view->stride : accessor->stride);
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

			primitives.push_back(outPrim);
		}

	cgltf_free(data);
}

void Model::render()
{
	for (const auto& prim : primitives)
	{
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

void Model::cleanup()
{
	for (auto& prim : primitives) {
		glDeleteVertexArrays(1, &prim.vao);
		glDeleteBuffers(1, &prim.vbo);
		if (prim.ibo) glDeleteBuffers(1, &prim.ibo);
	}
}