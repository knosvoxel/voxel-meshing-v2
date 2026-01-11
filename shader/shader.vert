#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in uint packed_data; // normal & color index

uniform mat4 mvp;
	
flat out uint color_idx;
flat out uint normal_idx;
	
void main()
{
    color_idx = packed_data & 255;
	normal_idx = (packed_data >> 8) & 7; // loweset 3 bits as only values from 0 - 5 are used

    gl_Position = mvp * vec4(aPos, 1.0);
}