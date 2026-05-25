#version 460
#extension GL_GOOGLE_include_directive : require              // For #include
#extension GL_EXT_scalar_block_layout : require               // For scalar layout
#extension GL_EXT_shader_explicit_arithmetic_types : require  // For uint64_t, ...
#extension GL_EXT_buffer_reference2 : require                 // For buffer reference

// Входы из вершинного буфера (соответствуют struct Vertex в C++)
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

// Выход во фрагментный шейдер
layout(location = 0) out vec3 fragColor;

void main() {
    // Позиция в клип-спейсе (Z = 0.0, W = 1.0)
    gl_Position = vec4(inPos, 0.0, 1.0);
    
    // Передаем цвет во фрагментный шейдер
    fragColor = inColor;
}