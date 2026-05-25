#version 450

#extension GL_EXT_nonuniform_qualifier : enable

// Входные переменные
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inFactor;
layout(location = 3) in vec3 inLightVec;
layout(location = 4) in vec3 inViewVec;
layout(location = 5) flat in uint inInstanceIndex;

// Дескрипторы текстур (массив текстур)
layout(set = 1, binding = 0) uniform sampler2D textures[];

// Выходной цвет
layout(location = 0) out vec4 outColor;

void main() {
    // Phong освещение
    vec3 N = normalize(inNormal);
    vec3 L = normalize(inLightVec);
    vec3 V = normalize(inViewVec);
    vec3 R = reflect(-L, N);
    
    float diffuse = max(dot(N, L), 0.0025);  // Это должно быть float, а не vec3!
    float specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;
    
    // Сэмплирование текстуры с использованием динамического индекса
    vec3 texColor = texture(textures[nonuniformEXT(inInstanceIndex)], inUV).rgb;
    vec3 color = texColor * inFactor;
    
    outColor = vec4(diffuse * color + specular, 1.0);
}