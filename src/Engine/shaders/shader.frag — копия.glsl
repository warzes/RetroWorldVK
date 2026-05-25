#version 460

#extension GL_GOOGLE_include_directive : require              // For #include
#extension GL_EXT_scalar_block_layout : require               // For scalar layout
#extension GL_EXT_shader_explicit_arithmetic_types : require  // For uint64_t, ...
#extension GL_EXT_buffer_reference2 : require                 // For buffer reference
#extension GL_EXT_nonuniform_qualifier : require              // For non-uniform indexing of the texture array
#extension GL_EXT_descriptor_heap : enable                    // For bindless descriptor heap access (VK_EXT_descriptor_heap)

// Вход из вершинного шейдера (интерполированный цвет)
layout(location = 0) in vec3 fragColor;

// Выход в цветовой аттачмент (Swapchain image)
layout(location = 0) out vec4 outColor;

void main() {
    // Добавляем альфа-канал (1.0) и записываем в выход
    outColor = vec4(fragColor, 1.0);
}