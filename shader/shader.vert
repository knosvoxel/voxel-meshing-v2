#version 460 core
#extension GL_NV_gpu_shader5 : enable

struct Vertex{
	float16_t x, y, z;
};

layout(binding = 0, std430) readonly restrict buffer vertexBuffer {
	Vertex vertices[];
};

layout(binding = 1, std430) readonly restrict buffer packedDataBuffer {
	uint packedData[];
};

layout(binding = 2, std430) readonly restrict buffer transformBuffer {
    mat4 transforms[];
};

uniform mat4 mvp;
	
flat out uint color_idx;
flat out uint normal_idx;
	
void main()
{
	Vertex v = vertices[gl_VertexID];
	vec3 pos = vec3(v.x, v.y, v.z);

	uint data = packedData[gl_VertexID / 4];

    color_idx = data & 255;
	normal_idx = (data >> 8) & 7; // loweset 3 bits as only values from 0 - 5 are used
	
	mat4 model = transforms[gl_BaseInstance];
	gl_Position = mvp * model * vec4(pos, 1.0);
}