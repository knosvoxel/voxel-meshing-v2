#version 460 core
#extension GL_NV_gpu_shader5 : enable

layout(binding = 0, std430) readonly restrict buffer vertexBuffer {
	uint vertices[];
};

uniform mat4 mvp;
	
flat out uint color_idx;
flat out uint normal_idx;
	
void main()
{
	uint v = vertices[gl_VertexID];
    vec3 pos = vec3(v & 63, (v >> 6) & 63, (v >> 12) & 63);

    color_idx  = (v >> 22) & 255;
	normal_idx = (v >> 18) & 7;

	gl_Position = mvp * vec4(pos, 1.0);
}