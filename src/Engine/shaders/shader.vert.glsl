#version 460
#extension GL_GOOGLE_include_directive : require              // For #include
#extension GL_EXT_scalar_block_layout : require               // For scalar layout
#extension GL_EXT_shader_explicit_arithmetic_types : require  // For uint64_t, ...
#extension GL_EXT_buffer_reference2 : require                 // For buffer reference

// Входные атрибуты
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Push constant block для индекса инстанса
layout(push_constant) uniform PushConstants {
    uint instanceIndex;
} pushConsts;

// Uniform buffer object
layout(set = 0, binding = 0) uniform ShaderData {
    mat4 projection;
    mat4 view;
    mat4 model[3];
    vec4 lightPos;
    uint selected;
} shaderData;

// Выходные переменные
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outFactor;
layout(location = 3) out vec3 outLightVec;
layout(location = 4) out vec3 outViewVec;
layout(location = 5) out uint outInstanceIndex;

void main() {
    uint instanceIdx = pushConsts.instanceIndex; // Используем push constant для индекса инстанса
    
    mat4 modelMat = shaderData.model[instanceIdx];
    
    // Преобразование нормали в видовую систему координат
    mat3 normalMatrix = mat3(transpose(inverse(shaderData.view * modelMat)));
    outNormal = normalMatrix * inNormal;
    
    outUV = inUV;
    
    // Полная трансформация позиции
    gl_Position = shaderData.projection * shaderData.view * modelMat * vec4(inPos, 1.0);
    
    // Определяем фактор освещения
    outFactor = (shaderData.selected == instanceIdx) ? vec3(3.0) : vec3(1.0);
    
    outInstanceIndex = instanceIdx;
    
    // Расчет векторов для освещения
    vec4 fragPos = shaderData.view * modelMat * vec4(inPos, 1.0);
    outLightVec = shaderData.lightPos.xyz - fragPos.xyz;
    outViewVec = -fragPos.xyz;
}