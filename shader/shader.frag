#version 450 core
out vec4 FragColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoord;

uniform vec3 light_direction;

void main() {
    vec3 light_col = vec3(1.0f);

    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * light_col;

    vec3 norm = normalize(vNormal);
    
    vec3 lightDir = normalize(-light_direction);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light_col;

    vec3 result = (ambient + diffuse);
    FragColor = vec4(result, 1.0f);
}