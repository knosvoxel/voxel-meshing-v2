#version 460 core
#extension GL_NV_gpu_shader5 : enable

layout(binding = 0, std430) readonly restrict buffer quadBuffer {
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

    int qx   = int( q         & 511ul);
    int qy   = int((q >>  9u) & 511ul);
    int qz   = int((q >> 18u) & 511ul);
    int qlen = int((q >> 27u) & 511ul);
    color_idx  = uint((q >> 36u) & 255ul);
    uint face  = uint((q >> 44u) &   7ul);
    normal_idx = face;

    // u runs along the stretch (face_length) axis
    // v runs along the fixed (1 unit) axis
    const int cu[6] = int[6](0, qlen, qlen, 0, qlen, 0);
    const int cv[6] = int[6](0,    0,    1, 0,     1, 1);

    bool reverse_winding = (face == 0 || face == 2 || face == 4);
    int u, v;
    if (reverse_winding) {
        const int cu_r[6] = int[6](0, qlen, qlen, 0,    0, qlen);
        const int cv_r[6] = int[6](0,    1,    0, 0,    1,    1);
        u = cu_r[vert_idx];
        v = cv_r[vert_idx];
    } else {
        u = cu[vert_idx];
        v = cv[vert_idx];
    }

    vec3 pos;
    switch (face)
    {
    case 2: case 3:  // -Z/+Z from slicing_x: stretch=X, fixed=Y
        pos = vec3(qx + u, qy + v, qz);
        break;
    case 0: case 1:  // -X/+X from slicing_y: stretch=Y, fixed=Z
        pos = vec3(qx, qy + u, qz + v);
        break;
    case 4: case 5:  // -Y/+Y from slicing_z: stretch=Z, fixed=X
        pos = vec3(qx + v, qy, qz + u);
        break;
    }

    gl_Position = mvp * transforms[gl_DrawID] * vec4(pos, 1.0);
}