VK_EXT_shader_object и VK_KHR_dynamic_rendering убрали необходимость в VkPipeline, VkRenderPass и VkFramebuffer

===============================================================================

VK_KHR_synchronization2 — Убийца бойлерплейта барьеров
В классическом Vulkan функции vkCmdPipelineBarrier и структуры VkImageMemoryBarrier требуют дублирования: стадии пайплайна передаются отдельно в функцию, а маски доступа — в структуру.
Что меняет: Объединяет стадии и доступы в одну структуру VkImageMemoryBarrier2 и использует функцию vkCmdPipelineBarrier2.
Экономия кода: Убирает необходимость писать обвязку для маппинга стадий, делает код барьеров короче на 30-40% и, главное, исключает классические ошибки рассинхрона.

// Было (Vulkan 1.0):
vkCmdPipelineBarrier(cmd, 
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Стадия отдельно
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // Стадия отдельно
    0, 0, nullptr, 0, nullptr, 1, &barrier);       // Куча нулей

// Стало (Vulkan 1.3+):
VkImageMemoryBarrier2 barrier2 = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // Всё в структуре
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
    // ...
};
vkCmdPipelineBarrier2(cmd, &dependencyInfo); // Никаких лишних параметров

===============================================================================

VK_KHR_push_descriptor — Полный отказ от Descriptor Pools и Sets
Если в вашем шейдере мало ресурсов (например, пара uniform-буферов или текстур), создание VkDescriptorSetLayout, VkDescriptorPool, аллокация сетов и их обновление (vkUpdateDescriptorSets) — это сотни строк кода.
Что меняет: Позволяет пушить дескрипторы напрямую в Command Buffer, минуя создание пулов и аллокацию сетов.
Экономия кода: Минус ~150 строк инициализации дескрипторов. Вы просто вызываете vkCmdPushDescriptorSetKHR при записи команд.

===============================================================================

VK_EXT_extended_dynamic_state (1, 2, 3) (Core в 1.3) — Конец «взрыву пайплайнов»
Примечание: тоже что VK_EXT_shader_object?.
Раньше изменение режима отсечения (cull mode), топологии (triangles/lines), теста глубины или полигонального режима (fill/wireframe) требовало создания нового VkGraphicsPipeline.
Что меняет: Делает почти всё состояние пайплайна динамическим (vkCmdSetCullMode, vkCmdSetPrimitiveTopology и т.д.).
Экономия кода: Вместо 20 разных VkPipeline для разных состояний вы создаете один и переключаете состояния командами в cmd.

===============================================================================

VK_KHR_maintenance5 (Core в 1.4) — Inline SPIR-V
В классическом Vulkan нужно сначала создать VkShaderModule из SPIR-V, а потом передать его в пайплайн.
Что меняет: Позволяет передать массив uint32_t (SPIR-V) напрямую в VkShaderModuleCreateInfo или сразу в создание пайплайна, пропуская шаг создания и уничтожения VkShaderModule.
Экономия кода: Убирает ~10 строк на каждый шейдер (создание/удаление модуля). С VK_EXT_shader_object это тоже работает и упрощает код.

===============================================================================

VK_EXT_swapchain_maintenance1 — Упрощение Resize и Present
Пересоздание Swapchain при ресайзе окна — это боль, потому что нужно обрабатывать VK_ERROR_OUT_OF_DATE_KHR в разных местах, а смена Present Mode (например, с V-Sync на безлимитный) требовала полного пересоздания.
Что меняет:
Позволяет менять VkPresentMode без пересоздания swapchain.
Добавляет VkSwapchainPresentModesCreateInfoEXT, позволяя драйверу самому выбирать фоллбек-режимы.
Дает явный ивент VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT, чтобы знать, когда изображение реально показано на экране (убирает необходимость в лишних семафорах для CPU-синхронизации).
Экономия кода: Делает логику recreateSwapchain намного надежнее и короче.

===============================================================================

VK_EXT_mutable_descriptor_type (Core в 1.3) — Упрощение Descriptor Pools
Если вы не используете Push Descriptors или Bindless, вам нужно точно указывать в VkDescriptorPoolSize, сколько именно UNIFORM_BUFFER, COMBINED_IMAGE_SAMPLER и т.д. вам нужно. Ошиблись — краш или VK_ERROR_OUT_OF_POOL_MEMORY.
Что меняет: Вводит VK_DESCRIPTOR_TYPE_MUTABLE_EXT. Вы можете создать пул из «универсальных» слотов и класть туда любой тип дескриптора.
Экономия кода: Убирает сложный подсчет ресурсов на CPU при инициализации пулов.

===============================================================================

 VK_EXT_host_image_copy (Core в Vulkan 1.4) — Убийца стейджинг-буферов
Проблема: В классическом Vulkan загрузка текстуры с CPU на GPU — это ад из ~100 строк кода. Нужно создать временный (staging) буфер, выделить под него память, замапить, скопировать данные, создать Image, выделить память под Image, сделать transition barrier (UNDEFINED -> TRANSFER_DST), записать команду vkCmdCopyBufferToImage, сделать еще один barrier (TRANSFER_DST -> SHADER_READ_ONLY), отправить в очередь, дождаться выполнения и уничтожить стейджинг-буфер.
Что меняет: Позволяет копировать данные из памяти CPU напрямую в VkImage на хосте, вообще не трогая Command Buffer и очереди.
Экономия кода: Минус ~80-100 строк на каждую текстуру.
// Было (Vulkan 1.0): Создание буфера, маппинг, барьеры, vkCmdCopyBufferToImage, очистка...

// Стало (Vulkan 1.4):
VkMemoryToImageCopyEXT region = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
    .pHostPointer = pixelData,
    .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
    .imageExtent = { width, height, 1 }
};
VkCopyMemoryToImageInfoEXT copyInfo = {
    .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
    .dstImage = textureImage,
    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .regionCount = 1, .pRegions = &region
};
vkCopyMemoryToImageEXT(device, &copyInfo); // Всё. Текстура загружена.

===============================================================================

VK_KHR_timeline_semaphore (Core в Vulkan 1.2) — Конец эхе бинарных семафоров
Проблема: Бинарные семафоры (VkSemaphore) в Vulkan 1.0 могут быть только в двух состояниях: signaled и unsignaled. Их нельзя сбросить вручную, и они требуют жесткого парного signal/wait. Из-за этого разработчикам приходится создавать пулы семафоров (по 2-3 на кадр) и писать сложную логику их ротации (frames-in-flight).
Что меняет: Вводит семафоры, значение которых — это просто число (uint64_t). Вы можете сказать GPU: «сигнализируй значение 5», а CPU: «жди, пока значение не станет 5».
Экономия кода: Убирает необходимость в массивах VkSemaphore и VkFence для каждого кадра. Одна глобальная пара таймлайн-семафоров может заменить десятки бинарных, делая синхронизацию CPU-GPU тривиальной.

// Ждем на CPU, пока GPU не дойдет до отрисовки кадра №42
VkSemaphoreWaitInfo waitInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1, .pSemaphores = &timelineSemaphore,
    .pValues = &frameNumber // Ждем конкретное число
};
vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

===============================================================================

Bindless-архитектура через VK_EXT_descriptor_indexing (Core в 1.2)
Проблема: В старом Vulkan для каждого объекта нужно создавать/обновлять VkDescriptorSet, биндить его перед отрисовкой и следить за жизненным циклом пулов. Это сотни строк менеджмента ресурсов.
Что меняет: Позволяет создать один гигантский Descriptor Set на всё приложение (например, массив из 100 000 текстур) и обновлять его асинхронно. В шейдер передается только индекс (через Push Constants).
Экономия кода: Полностью убирает классы DescriptorSetManager, DescriptorPool и логику биндинга сетов из рендер-лупа. Рендер-луп сводится к vkCmdPushConstants (передаче индекса) и vkCmdDraw.

===============================================================================

VK_KHR_dynamic_rendering_local_read (Core в Vulkan 1.4) — Убийца Subpasses
Проблема: Для Deferred Rendering или постпроцессинга, где нужно читать из одного аттачмента (например, G-Buffer) и писать в другой в рамках одного прохода, в Vulkan 1.0 использовались VkSubpass и Input Attachments. Это требовало жесткого описания в VkRenderPass и создания сложных VkSubpassDependency.
Что меняет: Позволяет внутри vkCmdBeginRenderingKHR динамически менять layout изображений и читать из них как из input-аттачментов, вообще не используя концепцию сабпассов.
Экономия кода: Удаляет концепцию Subpasses из кода. Deferred рендеринг теперь пишется так же просто, как и Forward, через обычные dynamic rendering барьеры.

===============================================================================

VK_EXT_debug_utils — Человеческий дебаг
Проблема: Старый VK_EXT_debug_report был громоздким, а при падении валидации с сообщением "VkImage 0x4523aa is used incorrectly" было невозможно понять, о какой именно текстуре идет речь.
Что меняет: Позволяет давать строковые имена любым объектам Vulkan прямо из кода.
Экономия кода: Вам больше не нужно писать собственные обертки (хэш-таблицы), чтобы мапить хендлы Vulkan на строки для логирования.

// Даем имя объекту
VkDebugUtilsObjectNameInfoEXT nameInfo = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = VK_OBJECT_TYPE_IMAGE,
    .objectHandle = (uint64_t)myTextureImage,
    .pObjectName = "Player_Diffuse_Texture"
};
vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
// Теперь валидация будет ругаться: "Player_Diffuse_Texture is used incorrectly"