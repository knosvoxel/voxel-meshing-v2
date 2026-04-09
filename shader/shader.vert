#version 460 core

struct Quad {
    float x, y, z;
    uint packedData;
};

layout(binding = 0, std430) readonly restrict buffer quadBuffer {
    Quad quads[];
};
layout(binding = 1, std430) readonly restrict buffer transformBuffer {
    mat4 transforms[];
};

uniform mat4 mvp;

flat out uint color_idx;
flat out uint normal_idx;

const vec3 vertex_offsets[36] = vec3[36](
    // Normal 0: -X. Quad in YZ plane. fixed=+Z, stretch=+Y.
    // CCW from -X: c0,c1,c2 | c0,c2,c3
    vec3(0,0,0), vec3(0,0,1), vec3(0,2,1),
    vec3(0,0,0), vec3(0,2,1), vec3(0,2,0),

    // Normal 1: +X. x+1 baked in base. Flip winding.
    // CCW from +X: c0,c3,c2 | c0,c2,c1
    vec3(0,0,0), vec3(0,2,0), vec3(0,2,1),
    vec3(0,0,0), vec3(0,2,1), vec3(0,0,1),

    // Normal 2: -Z. Quad in XY plane. fixed=+Y, stretch=+X.
    // CCW from -Z: c0,c1,c2 | c0,c2,c3
    vec3(0,0,0), vec3(0,1,0), vec3(2,1,0),
    vec3(0,0,0), vec3(2,1,0), vec3(2,0,0),

    // Normal 3: +Z. z+1 baked in base. Flip winding.
    // CCW from +Z: c0,c3,c2 | c0,c2,c1
    vec3(0,0,0), vec3(2,0,0), vec3(2,1,0),
    vec3(0,0,0), vec3(2,1,0), vec3(0,1,0),

    // Normal 4: -Y. Quad in XZ plane. fixed=+X, stretch=+Z.
    // CCW from -Y: c0,c1,c2 | c0,c2,c3
    vec3(0,0,0), vec3(1,0,0), vec3(1,0,2),
    vec3(0,0,0), vec3(1,0,2), vec3(0,0,2),

    // Normal 5: +Y. y+1 baked in base. Flip winding.
    // CCW from +Y: c0,c3,c2 | c0,c2,c1
    vec3(0,0,0), vec3(0,0,2), vec3(1,0,2),
    vec3(0,0,0), vec3(1,0,2), vec3(1,0,0)
);

void main()
{
    uint quad_idx   = uint(gl_VertexID) / 6u;
    uint vertex_idx = uint(gl_VertexID) % 6u;

    Quad q = quads[quad_idx];
    vec3 base = vec3(q.x, q.y, q.z);

    color_idx  =  q.packedData        & 255u;
    uint n_idx = (q.packedData >> 8u) &   7u;
    normal_idx = n_idx;
    float face_length = float(q.packedData >> 16u);

    vec3 mask = vertex_offsets[n_idx * 6u + vertex_idx];

    vec3 offset = vec3(
        mask.x > 1.5 ? face_length : mask.x,
        mask.y > 1.5 ? face_length : mask.y,
        mask.z > 1.5 ? face_length : mask.z
    );

    vec3 final_pos = base + offset;

    mat4 model = transforms[gl_BaseInstance];
    gl_Position = mvp * model * vec4(final_pos, 1.0);
}