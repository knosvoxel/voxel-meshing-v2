#version 460 core
#extension GL_NV_gpu_shader5 : enable

layout(binding = 0, std430) readonly restrict buffer vertexBuffer {
	uint64_t quads[];
};

layout(binding = 1, std430) readonly restrict buffer transformBuffer {
	mat4 transforms[];
};

uniform mat4 mvp;
	
flat out uint color_idx;
flat out uint normal_idx;
	
void main()
{
    int quad_idx = gl_VertexID / 6;
    int vert_idx = gl_VertexID % 6;

    uint64_t q = quads[quad_idx];
    int qx = int( q        & 63ul);
    int qy = int((q >>  6) & 63ul);
    int qz = int((q >> 12) & 63ul);
    int qw = int((q >> 18) & 63ul);  // width
    int qh = int((q >> 24) & 63ul);  // height
    color_idx  = uint((q >> 32) & 255ul);
    uint face  =  uint((q >> 40) & 7ul);
    normal_idx = face;

    // quad corner offsets for a triangle list (2 triangles, 6 verts)
    // corners: 0=(0,0), 1=(w,0), 2=(w,h), 3=(0,0), 4=(w,h), 5=(0,h)
    const int cu[6] = int[6](0, qw, qw,  0, qw,  0);
    const int cv[6] = int[6](0,  0, qh,  0, qh, qh);
    
    bool reverse_winding = (face == 0 || face == 2 || face == 5); // adjust per your results
    int u, v;
    if (reverse_winding) {
        const int cu_r[6] = int[6](0, qw, qw,  0,  0, qw); // swapped
        const int cv_r[6] = int[6](0, qh,  0,  0, qh, qh);
        u = cu_r[vert_idx];
        v = cv_r[vert_idx];
    } else {
        u = cu[vert_idx];
        v = cv[vert_idx];
    }

    vec3 pos;
    switch (face)
    {
    case 0: case 1:  // Y faces: expand in X and Z
        pos = vec3(qx + u, qy, qz + v);
        break;
    case 2: case 3:  // X faces: expand in Z and Y
        pos = vec3(qx, qy + v, qz + u);
        break;
    case 4: case 5:  // Z faces: expand in X and Y
        pos = vec3(qx + u, qy + v, qz);
        break;
    }

    gl_Position = mvp * transforms[gl_DrawID] * vec4(pos, 1.0);
}