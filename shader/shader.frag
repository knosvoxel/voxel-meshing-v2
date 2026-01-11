#version 460 core
out vec4 FragColor;
	
flat in uint color_idx;
flat in uint normal_idx;
	
uniform sampler2D palette;
uniform vec3 light_direction;

const vec3 normal_directions[6] = vec3[6](
    vec3(-1.0,  0.0,  0.0),  // 0
    vec3( 1.0,  0.0,  0.0),  // 1
    vec3( 0.0,  0.0, -1.0),  // 2
    vec3( 0.0,  0.0,  1.0),  // 3
    vec3( 0.0, -1.0,  0.0),  // 4
    vec3( 0.0,  1.0,  0.0)   // 5
);
	
void main()
{         
    vec3 texCol = texture(palette, vec2(color_idx / 256.0, 0.5)).rgb;      
    vec3 light_col = vec3(1.0f);

    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * light_col;

	vec3 norm = normalize(normal_directions[normal_idx]);
    vec3 lightDir = normalize(-light_direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light_col;

    vec3 result = (ambient + diff) * texCol;

    FragColor = vec4(result, 1.0f);
}