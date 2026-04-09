#version 450 core
out vec4 FragColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoord;

uniform vec3 light_direction;
uniform sampler2D uTexture;

uniform vec2 uTexOffset;
uniform vec2 uTexScale;

void main() {
    vec2 uv = vTexCoord * uTexScale + uTexOffset;
    vec4 albedo = texture(uTexture, uv);
    vec3 light_col = vec3(1.0f);

    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * light_col;

    vec3 norm = normalize(vNormal);
    
    vec3 lightDir = normalize(-light_direction);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light_col;

    vec3 result = albedo.rgb * (ambient + diffuse);
    FragColor = vec4(result.rgb, 1.0f);
}