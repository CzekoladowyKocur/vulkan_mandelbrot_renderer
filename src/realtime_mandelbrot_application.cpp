#include "include/realtime_mandelbrot_application.hpp"
#include "include/input.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <print>
#include <span>
#include <string>
#include <string_view>

namespace {
constexpr uint64_t max_swapchain_timeout{UINT64_MAX};
} // namespace

bool realtime_mandelbrot_application::initialize() {
  auto context{vulkan_context::create(
      {.instance_extensions{window::get_required_extensions()}})};
  if (!context) {
    std::println("Failed to create vulkan context: {}",
                 context.error().message());
    return false;
  }

  m_context = std::move(*context);

  if (!load_assets()) {
    std::println("Failed to load assets");
    return false;
  }

  auto created{window::create({.width{1280u},
                               .height{720u},
                               .name{"Vulkan Mandelbrot Set Renderer"},
                               .maximized{true}})};

  if (!created) {
    std::println("Failed to initialize window: {}", created.error().message());

    return false;
  }

  m_window = std::move(*created);

  if (!create_surface()) {
    std::println("Failed to create vulkan surface");
    return false;
  }

  if (!create_swapchain()) {
    std::println("Failed to create vulkan swapchain");
    return false;
  }

  if (!create_graphics_based_pipeline()) {
    std::println("Failed to create graphics based pipeline");
    return false;
  }

  if (!allocate_graphics_command_buffers()) {
    std::println("Failed to allocate graphics command buffers");
    return false;
  }

  if (!record_graphics_command_buffers()) {
    std::println("Failed to create graphics command buffers");
    return false;
  }

  return true;
}

bool realtime_mandelbrot_application::run() {
  if (!m_window.has_value()) {
    return false;
  }

  window &active_window{*m_window};

  auto previous_time{std::chrono::steady_clock::now()};

  while (m_running) {
    /* Poll events */

    active_window.poll([this](const event &polled) { on_event(polled); });
    const auto current_time{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> delta_time{current_time - previous_time};
    previous_time = current_time;

    update_frame_data(delta_time.count());
    draw_frame();
  }

  return true;
}

bool realtime_mandelbrot_application::shutdown() {
  VK_CHECK(vkDeviceWaitIdle(m_context.device()));
  m_color_palette_texture.reset();
  /* Device level */
  VK_CHECK(vkDeviceWaitIdle(m_context.device()));

  /* Graphics */
  /* Destroy buffers */
  m_ubo_buffer.reset();
  m_vertex_buffer.reset();
  m_index_buffer.reset();

  /* Destroy Pipelines */
  if (m_graphics_pipeline)
    vkDestroyPipeline(m_context.device(), m_graphics_pipeline, nullptr);

  if (m_graphics_pipeline_layout)
    vkDestroyPipelineLayout(m_context.device(), m_graphics_pipeline_layout,
                            nullptr);

  if (m_graphics_pipeline_ubo_buffer_descriptor_set_layout)
    vkDestroyDescriptorSetLayout(
        m_context.device(),
        m_graphics_pipeline_ubo_buffer_descriptor_set_layout, nullptr);

  if (m_graphics_pipeline_color_palette_descriptor_set_layout)
    vkDestroyDescriptorSetLayout(
        m_context.device(),
        m_graphics_pipeline_color_palette_descriptor_set_layout, nullptr);

  if (m_graphics_pipeline_color_palette_descriptor_set)
    vkFreeDescriptorSets(m_context.device(),
                         m_graphics_pipeline_descriptor_pool, 1,
                         &m_graphics_pipeline_color_palette_descriptor_set);

  if (m_graphics_pipeline_descriptor_pool)
    vkDestroyDescriptorPool(m_context.device(),
                            m_graphics_pipeline_descriptor_pool, nullptr);

  cleanup_swapchain();

  /* Instance level */
  vkDestroySurfaceKHR(m_context.instance(), m_surface, nullptr);

  m_context = {};

  return true;
}

void realtime_mandelbrot_application::on_event(const event &polled_event) {
  std::visit(overloaded{
                 [this](const window_close_event &) { m_running = false; },

                 [this](const window_resize_event &resize) {
                   m_swapchain_extent.width = resize.width;
                   m_swapchain_extent.height = resize.height;

                   std::println("Window resized: [width, height]: {}, {}",
                                resize.width, resize.height);
                 },

                 [](const window_minimize_event &) {},

                 [this](const key_pressed_event &key) {
                   m_input.set_key_state(key.code, true);
                 },

                 [this](const key_released_event &key) {
                   m_input.set_key_state(key.code, false);
                 },
             },
             polled_event);
}

bool realtime_mandelbrot_application::create_surface() {
  if (!m_window.has_value()) {
    return false;
  }

  const auto surface{m_window->get_surface(m_context.instance())};

  if (!surface) {
    std::println("Failed to create vulkan surface: {}",
                 surface.error().message());
    return false;
  }

  m_surface = *surface;

  VkBool32 supported;
  vkGetPhysicalDeviceSurfaceSupportKHR(m_context.physical_device(),
                                       m_context.graphics_queue_family(),
                                       m_surface, &supported);
  return true;
}

bool realtime_mandelbrot_application::create_swapchain() {
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      m_context.physical_device(), m_surface, &m_surface_capabilities));

  uint32_t surface_format_count;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_context.physical_device(), m_surface, &surface_format_count, nullptr));
  assert(surface_format_count > 0);

  std::vector<VkSurfaceFormatKHR> available_surface_formats(
      surface_format_count);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_context.physical_device(), m_surface, &surface_format_count,
      available_surface_formats.data()));

  m_surface_format = available_surface_formats[0];
  for (const VkSurfaceFormatKHR surface_format : available_surface_formats)
    if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      m_surface_format = surface_format;
      break;
    }

  uint32_t present_mode_count;
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_context.physical_device(), m_surface, &present_mode_count, nullptr));
  assert(present_mode_count > 0);

  std::vector<VkPresentModeKHR> available_present_modes(present_mode_count);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_context.physical_device(), m_surface, &present_mode_count,
      available_present_modes.data()));

  /* The only present mode guaranteed to be supported by the specification */
  m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (const VkPresentModeKHR present_mode : available_present_modes)
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      m_present_mode = present_mode;
      break;
    }

  m_max_frames_in_flight =
      m_present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? (3) : (2);

  const VkExtent2D min_swapchain_image_extent =
      m_surface_capabilities.minImageExtent;
  const VkExtent2D max_swapchain_image_extent =
      m_surface_capabilities.maxImageExtent;

  /* Clamp */
  m_swapchain_extent.width =
      std::clamp(m_swapchain_extent.width, min_swapchain_image_extent.width,
                 max_swapchain_image_extent.width);
  m_swapchain_extent.height =
      std::clamp(m_swapchain_extent.height, min_swapchain_image_extent.height,
                 max_swapchain_image_extent.height);

  const uint32_t min_image_count = m_surface_capabilities.minImageCount;
  const uint32_t max_image_count = m_surface_capabilities.maxImageCount;
  m_image_count = (min_image_count + 1) < max_image_count
                      ? (min_image_count + 1)
                      : max_image_count;

  const VkSwapchainCreateInfoKHR swapchain_create_info{
      .sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
      .pNext{nullptr},
      .flags{},
      .surface{m_surface},
      .minImageCount{m_image_count},
      .imageFormat{m_surface_format.format},
      .imageColorSpace{m_surface_format.colorSpace},
      .imageExtent{m_swapchain_extent},
      .imageArrayLayers{1u},
      .imageUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
      .imageSharingMode{VK_SHARING_MODE_EXCLUSIVE},
      .queueFamilyIndexCount{},
      .pQueueFamilyIndices{nullptr},
      .preTransform{m_surface_capabilities.currentTransform},
      .compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
      .presentMode{m_present_mode},
      .clipped{VK_TRUE},
      .oldSwapchain{VK_NULL_HANDLE}};

  if (vkCreateSwapchainKHR(m_context.device(), &swapchain_create_info, nullptr,
                           &m_swapchain) != VK_SUCCESS) {
    std::println("Failed to create swapchain");
    return false;
  }

  m_image_count = 0;
  m_swapchain_images.clear();
  VK_CHECK(vkGetSwapchainImagesKHR(m_context.device(), m_swapchain,
                                   &m_image_count, nullptr));

  m_swapchain_images.resize(m_image_count);
  m_swapchain_image_views.resize(m_image_count);
  m_swapchain_framebuffers.resize(m_image_count);

  VK_CHECK(vkGetSwapchainImagesKHR(m_context.device(), m_swapchain,
                                   &m_image_count, m_swapchain_images.data()));

  const VkAttachmentDescription color_attachment{
      .flags{},
      .format{m_surface_format.format},
      .samples{VK_SAMPLE_COUNT_1_BIT},
      .loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR},
      .storeOp{VK_ATTACHMENT_STORE_OP_STORE},
      .stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE},
      .stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE},
      .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED},
      .finalLayout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

  const VkAttachmentReference color_attachment_reference{
      .attachment{}, .layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  const VkSubpassDescription subpass_description{
      .flags{},
      .pipelineBindPoint{VK_PIPELINE_BIND_POINT_GRAPHICS},
      .inputAttachmentCount{},
      .pInputAttachments{nullptr},
      .colorAttachmentCount{1u},
      .pColorAttachments{&color_attachment_reference},
      .pResolveAttachments{nullptr},
      .pDepthStencilAttachment{nullptr},
      .preserveAttachmentCount{},
      .pPreserveAttachments{nullptr}};

  const VkSubpassDependency subpass_dependency{
      .srcSubpass{VK_SUBPASS_EXTERNAL},
      .dstSubpass{},
      .srcStageMask{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
      .dstStageMask{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
      .srcAccessMask{},
      .dstAccessMask{VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
      .dependencyFlags{}};

  const std::array<VkAttachmentDescription, 1> attachments{color_attachment};
  const VkRenderPassCreateInfo render_pass_create_info{
      .sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .attachmentCount{static_cast<uint32_t>(attachments.size())},
      .pAttachments{attachments.data()},
      .subpassCount{1u},
      .pSubpasses{&subpass_description},
      .dependencyCount{1u},
      .pDependencies{&subpass_dependency}};

  VK_CHECK(vkCreateRenderPass(m_context.device(), &render_pass_create_info,
                              nullptr, &m_swapchain_render_pass));

  uint32_t image_index = 0;
  for (const VkImage image : m_swapchain_images) {
    const VkImageViewCreateInfo image_view_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .image{image},
        .viewType{VK_IMAGE_VIEW_TYPE_2D},
        .format{m_surface_format.format},
        .components{.r{VK_COMPONENT_SWIZZLE_R},
                    .g{VK_COMPONENT_SWIZZLE_G},
                    .b{VK_COMPONENT_SWIZZLE_B},
                    .a{VK_COMPONENT_SWIZZLE_A}},
        .subresourceRange{.aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                          .baseMipLevel{},
                          .levelCount{1u},
                          .baseArrayLayer{},
                          .layerCount{1u}}};

    VK_CHECK(vkCreateImageView(m_context.device(), &image_view_create_info,
                               nullptr, &m_swapchain_image_views[image_index]));

    const VkFramebufferCreateInfo framebuffer_create_info{
        .sType{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .renderPass{m_swapchain_render_pass},
        .attachmentCount{1u},
        .pAttachments{&m_swapchain_image_views[image_index]},
        .width{m_swapchain_extent.width},
        .height{m_swapchain_extent.height},
        .layers{1u}};

    VK_CHECK(vkCreateFramebuffer(m_context.device(), &framebuffer_create_info,
                                 nullptr,
                                 &m_swapchain_framebuffers[image_index]));

    ++image_index;
  }

  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
      .pNext{nullptr},
      .flags{}};

  const VkFenceCreateInfo fence_create_info{
      .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
      .pNext{nullptr},
      .flags{VK_FENCE_CREATE_SIGNALED_BIT}};

  m_semaphores.present_complete.resize(m_max_frames_in_flight);
  m_semaphores.render_complete.resize(m_max_frames_in_flight);
  m_in_flight_fences.resize(m_max_frames_in_flight);
  for (uint32_t i = 0; i < m_max_frames_in_flight; ++i) {
    VK_CHECK(vkCreateSemaphore(m_context.device(), &semaphore_create_info,
                               nullptr, &m_semaphores.present_complete[i]));

    VK_CHECK(vkCreateSemaphore(m_context.device(), &semaphore_create_info,
                               nullptr, &m_semaphores.render_complete[i]));

    VK_CHECK(vkCreateFence(m_context.device(), &fence_create_info, nullptr,
                           &m_in_flight_fences[i]));
  }

  m_images_in_flight.resize(m_image_count, VK_NULL_HANDLE);
  return true;
}

bool realtime_mandelbrot_application::load_assets() {
  const auto palette_image{image_2d::create("assets/images/violetPalette.bmp")};
  if (!palette_image) {
    return false;
  }

  auto palette{
      texture_2d::create({.image{*palette_image},
                          .device{m_context.device()},
                          .physical_device{m_context.physical_device()},
                          .command_pool{m_context.graphics_command_pool()},
                          .queue{m_context.graphics_queue()}})};
  if (!palette) {
    return false;
  }

  m_color_palette_texture.emplace(std::move(*palette));

  return true;
}

bool realtime_mandelbrot_application::create_graphics_based_pipeline() {
  constexpr VkDeviceSize vertex_buffer_size = sizeof(float) * 4 * 3;
  constexpr VkDeviceSize index_buffer_size = sizeof(uint32_t) * 6;

  const std::array<float, 4ull * 3ull> fullscreen_quad_vertices

      {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
       1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};

  const std::array<uint32_t, 6ull> fullscreen_quad_indices{0, 1, 2, 2, 3, 0};

  const single_time_command_context upload_context{
      .device{m_context.device()},
      .command_pool{m_context.graphics_command_pool()},
      .queue{m_context.graphics_queue()}};

  {
    auto staging{vulkan_buffer::create(
        {.size{vertex_buffer_size},
         .device{m_context.device()},
         .physical_device{m_context.physical_device()},
         .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};

    if (!staging) {
      return false;
    }

    if (!staging->write(std::as_bytes(std::span{fullscreen_quad_vertices}))) {
      return false;
    }

    auto vertex_buffer{vulkan_buffer::create(
        {.size{vertex_buffer_size},
         .device{m_context.device()},
         .physical_device{m_context.physical_device()},
         .usage{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}})};

    if (!vertex_buffer) {
      return false;
    }

    if (!copy_buffer(upload_context, staging->handle(), vertex_buffer->handle(),
                     vertex_buffer_size)) {
      return false;
    }

    m_vertex_buffer.emplace(std::move(*vertex_buffer));
  }

  {
    auto staging{vulkan_buffer::create(
        {.size{index_buffer_size},
         .device{m_context.device()},
         .physical_device{m_context.physical_device()},
         .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
    if (!staging) {
      return false;
    }

    if (!staging->write(std::as_bytes(std::span{fullscreen_quad_indices}))) {
      return false;
    }

    auto index_buffer{vulkan_buffer::create(
        {.size{index_buffer_size},
         .device{m_context.device()},
         .physical_device{m_context.physical_device()},
         .usage{VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}})};
    if (!index_buffer) {
      return false;
    }

    if (!copy_buffer(upload_context, staging->handle(), index_buffer->handle(),
                     index_buffer_size)) {
      return false;
    }

    m_index_buffer.emplace(std::move(*index_buffer));
  }

  /* TODO: Add support for doubles */
  const bool device_supports_double_precision_floats =
      false; // m_context.physical_device()Features.shaderFloat64;
  const auto vertex_shader_module{create_shader_module(
      m_context.device(), device_supports_double_precision_floats
                              ? "assets/shaders/vertexShaderDoublePrecision.spv"
                              : "assets/shaders/vertexShader.spv")};
  if (!vertex_shader_module) {
    std::println("Failed to create vertex shader module");
    return false;
  }
  m_vertex_shader_module = *vertex_shader_module;

  const auto fragment_shader_module{create_shader_module(
      m_context.device(),
      device_supports_double_precision_floats
          ? "assets/shaders/fragmentShaderDoublePrecision.spv"
          : "assets/shaders/fragmentShader.spv")};
  if (!fragment_shader_module) {
    std::println("Failed to create fragment shader module");
    return false;
  }
  m_fragment_shader_module = *fragment_shader_module;

  VkPipelineShaderStageCreateInfo vertex_shader_stage_info;
  vertex_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_shader_stage_info.module = m_vertex_shader_module;
  vertex_shader_stage_info.pName = "main";
  vertex_shader_stage_info.pSpecializationInfo = nullptr;
  vertex_shader_stage_info.flags = 0;
  vertex_shader_stage_info.pNext = nullptr;

  VkPipelineShaderStageCreateInfo fragment_shader_stage_info;
  fragment_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_shader_stage_info.module = m_fragment_shader_module;
  fragment_shader_stage_info.pName = "main";
  fragment_shader_stage_info.pSpecializationInfo = nullptr;
  fragment_shader_stage_info.flags = 0;
  fragment_shader_stage_info.pNext = nullptr;

  const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{
      vertex_shader_stage_info, fragment_shader_stage_info};
  VkVertexInputBindingDescription vertex_input_binding_description;
  vertex_input_binding_description.binding = 0;
  vertex_input_binding_description.stride = sizeof(float) * 3;
  vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertex_input_attribute_description;
  vertex_input_attribute_description.binding = 0;
  vertex_input_attribute_description.location = 0;
  vertex_input_attribute_description.offset = 0;
  vertex_input_attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;

  VkPipelineVertexInputStateCreateInfo vertex_input_info;
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions =
      &vertex_input_binding_description;
  vertex_input_info.vertexAttributeDescriptionCount = 1;
  vertex_input_info.pVertexAttributeDescriptions =
      &vertex_input_attribute_description;
  vertex_input_info.flags = 0;
  vertex_input_info.pNext = nullptr;

  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;
  input_assembly.flags = 0;
  input_assembly.pNext = nullptr;

  VkViewport viewport;
  viewport.width = static_cast<float>(m_swapchain_extent.width);
  viewport.height = static_cast<float>(m_swapchain_extent.height);
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset = {.x{0}, .y{0}};
  scissor.extent = m_swapchain_extent;
  VkPipelineViewportStateCreateInfo viewport_state;
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;
  viewport_state.flags = 0;
  viewport_state.pNext = nullptr;

  const std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state_info;
  dynamic_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_info.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state_info.pDynamicStates = dynamic_states.data();
  dynamic_state_info.flags = 0;
  dynamic_state_info.pNext = nullptr;

  VkPipelineRasterizationStateCreateInfo rasterizer_state_info{};
  rasterizer_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer_state_info.depthClampEnable = VK_FALSE;
  rasterizer_state_info.rasterizerDiscardEnable = VK_FALSE;
  rasterizer_state_info.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer_state_info.lineWidth = 1.0f;
  rasterizer_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer_state_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer_state_info.depthBiasEnable = VK_FALSE;
  rasterizer_state_info.flags = 0;
  rasterizer_state_info.pNext = nullptr;

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
      .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT}};
  multisampling.sampleShadingEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;
  color_blending.blendConstants[0] = 0.0f;
  color_blending.blendConstants[1] = 0.0f;
  color_blending.blendConstants[2] = 0.0f;
  color_blending.blendConstants[3] = 0.0f;

  {
    VkDescriptorSetLayoutBinding ubo_binding;
    ubo_binding.binding = 0;
    ubo_binding.descriptorCount = 1;
    ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_binding.pImmutableSamplers = nullptr;

    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{ubo_binding};
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount =
        static_cast<uint32_t>(bindings.size());
    descriptor_set_layout_create_info.pBindings = bindings.data();
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.pNext = nullptr;

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_context.device(), &descriptor_set_layout_create_info, nullptr,
        &m_graphics_pipeline_ubo_buffer_descriptor_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding color_palette_binding;
    color_palette_binding.binding = 0;
    color_palette_binding.descriptorCount = 1;
    color_palette_binding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    color_palette_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    color_palette_binding.pImmutableSamplers = nullptr;

    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{
        color_palette_binding};
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount =
        static_cast<uint32_t>(bindings.size());
    descriptor_set_layout_create_info.pBindings = bindings.data();
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.pNext = nullptr;

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_context.device(), &descriptor_set_layout_create_info, nullptr,
        &m_graphics_pipeline_color_palette_descriptor_set_layout));
  }

  const std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts{
      m_graphics_pipeline_ubo_buffer_descriptor_set_layout,
      m_graphics_pipeline_color_palette_descriptor_set_layout};
  VkPipelineLayoutCreateInfo pipeline_layout_info;
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount =
      static_cast<uint32_t>(descriptor_set_layouts.size());
  pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
  pipeline_layout_info.pushConstantRangeCount = 0;
  pipeline_layout_info.pPushConstantRanges = nullptr;
  pipeline_layout_info.flags = 0;
  pipeline_layout_info.pNext = nullptr;

  if (!m_color_palette_texture.has_value()) {
    return false;
  }

  VkDescriptorImageInfo image_info;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = m_color_palette_texture->image_view();
  image_info.sampler = m_color_palette_texture->sampler();

  VkDescriptorPoolSize ubo_buffer_descriptor_pool_size;
  ubo_buffer_descriptor_pool_size.descriptorCount = 1;
  ubo_buffer_descriptor_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  VkDescriptorPoolSize color_palette_image_descriptor_pool_size;
  color_palette_image_descriptor_pool_size.descriptorCount = 1;
  color_palette_image_descriptor_pool_size.type =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

  const std::array<VkDescriptorPoolSize, 2> descriptor_pool_sizes{
      ubo_buffer_descriptor_pool_size,
      color_palette_image_descriptor_pool_size};
  VkDescriptorPoolCreateInfo descriptor_pool_create_info;
  descriptor_pool_create_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.poolSizeCount =
      static_cast<uint32_t>(descriptor_pool_sizes.size());
  descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes.data();
  descriptor_pool_create_info.maxSets = 10;
  descriptor_pool_create_info.flags =
      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptor_pool_create_info.pNext = nullptr;

  VK_CHECK(vkCreateDescriptorPool(m_context.device(),
                                  &descriptor_pool_create_info, nullptr,
                                  &m_graphics_pipeline_descriptor_pool));

  VkDescriptorSetAllocateInfo ubo_buffer_descriptor_set_allocate_info;
  ubo_buffer_descriptor_set_allocate_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ubo_buffer_descriptor_set_allocate_info.descriptorPool =
      m_graphics_pipeline_descriptor_pool;
  ubo_buffer_descriptor_set_allocate_info.descriptorSetCount = 1;
  ubo_buffer_descriptor_set_allocate_info.pSetLayouts =
      &m_graphics_pipeline_ubo_buffer_descriptor_set_layout;
  ubo_buffer_descriptor_set_allocate_info.pNext = nullptr;

  VK_CHECK(vkAllocateDescriptorSets(
      m_context.device(), &ubo_buffer_descriptor_set_allocate_info,
      &m_graphics_pipeline_ubo_buffer_descriptor_set));

  VkDescriptorSetAllocateInfo color_palette_image_descriptor_set_allocate_info;
  color_palette_image_descriptor_set_allocate_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  color_palette_image_descriptor_set_allocate_info.descriptorPool =
      m_graphics_pipeline_descriptor_pool;
  color_palette_image_descriptor_set_allocate_info.descriptorSetCount = 1;
  color_palette_image_descriptor_set_allocate_info.pSetLayouts =
      &m_graphics_pipeline_color_palette_descriptor_set_layout;
  color_palette_image_descriptor_set_allocate_info.pNext = nullptr;

  VK_CHECK(vkAllocateDescriptorSets(
      m_context.device(), &color_palette_image_descriptor_set_allocate_info,
      &m_graphics_pipeline_color_palette_descriptor_set));

  auto ubo_buffer{vulkan_buffer::create(
      {.size{sizeof(uniform_buffer_object)},
       .device{m_context.device()},
       .physical_device{m_context.physical_device()},
       .usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
  if (!ubo_buffer) {
    return false;
  }

  m_ubo_buffer.emplace(std::move(*ubo_buffer));

  VkWriteDescriptorSet color_palette_descriptor_set_write{};
  color_palette_descriptor_set_write.sType =
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  color_palette_descriptor_set_write.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  color_palette_descriptor_set_write.dstBinding = 0;
  color_palette_descriptor_set_write.dstArrayElement = 0;
  color_palette_descriptor_set_write.descriptorCount = 1;
  color_palette_descriptor_set_write.dstSet =
      m_graphics_pipeline_color_palette_descriptor_set;
  color_palette_descriptor_set_write.pBufferInfo = nullptr;
  color_palette_descriptor_set_write.pImageInfo = &image_info;
  color_palette_descriptor_set_write.pTexelBufferView = nullptr;
  color_palette_descriptor_set_write.pNext = nullptr;

  vkUpdateDescriptorSets(m_context.device(), 1,
                         &color_palette_descriptor_set_write, 0, nullptr);

  const auto temporary = glm::mat4(1.0f);
  if (!m_ubo_buffer->write(std::as_bytes(std::span{&temporary, 1})
                               .first(sizeof(uniform_buffer_object)))) {
    return false;
  }

  VkDescriptorBufferInfo buffer_info;
  buffer_info.buffer = m_ubo_buffer->handle();
  buffer_info.range = sizeof(uniform_buffer_object);
  buffer_info.offset = 0;

  VkWriteDescriptorSet ubo_buffer_descriptor_set_write{};
  ubo_buffer_descriptor_set_write.sType =
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  ubo_buffer_descriptor_set_write.descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_buffer_descriptor_set_write.dstBinding = 0;
  ubo_buffer_descriptor_set_write.dstArrayElement = 0;
  ubo_buffer_descriptor_set_write.descriptorCount = 1;
  ubo_buffer_descriptor_set_write.dstSet =
      m_graphics_pipeline_ubo_buffer_descriptor_set;
  ubo_buffer_descriptor_set_write.pBufferInfo = &buffer_info;
  ubo_buffer_descriptor_set_write.pImageInfo = nullptr;
  ubo_buffer_descriptor_set_write.pTexelBufferView = nullptr;
  ubo_buffer_descriptor_set_write.pNext = nullptr;

  vkUpdateDescriptorSets(m_context.device(), 1,
                         &ubo_buffer_descriptor_set_write, 0, nullptr);

  if (vkCreatePipelineLayout(m_context.device(), &pipeline_layout_info, nullptr,
                             &m_graphics_pipeline_layout) != VK_SUCCESS) {
    std::println("Failed to create graphics pipeline layout");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer_state_info;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.layout = m_graphics_pipeline_layout;
  pipeline_info.renderPass = m_swapchain_render_pass;
  pipeline_info.pDynamicState = &dynamic_state_info;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_info.subpass = 0;

  if (vkCreateGraphicsPipelines(m_context.device(), VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &m_graphics_pipeline) != VK_SUCCESS) {
    std::println("Failed to create graphics pipeline");
    return false;
  }

  vkDestroyShaderModule(m_context.device(), m_fragment_shader_module, nullptr);

  vkDestroyShaderModule(m_context.device(), m_vertex_shader_module, nullptr);

  return true;
}

bool realtime_mandelbrot_application::allocate_graphics_command_buffers() {
  VkCommandBufferAllocateInfo command_buffer_allocate_info;
  command_buffer_allocate_info.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_allocate_info.commandPool = m_context.graphics_command_pool();
  command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_allocate_info.commandBufferCount = m_image_count;
  command_buffer_allocate_info.pNext = nullptr;

  m_graphics_pipeline_command_buffers.resize(m_image_count);
  VK_CHECK(vkAllocateCommandBuffers(
      m_context.device(), &command_buffer_allocate_info,
      m_graphics_pipeline_command_buffers.data()));

  return true;
}

bool realtime_mandelbrot_application::record_graphics_command_buffers() {
  if (!m_vertex_buffer.has_value() || !m_index_buffer.has_value()) {
    return false;
  }

  for (uint32_t i = 0; i < m_image_count; ++i) {
    VkCommandBuffer &command_buffer = m_graphics_pipeline_command_buffers[i];
    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pInheritanceInfo = nullptr;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pNext = nullptr;

    VkClearValue color_clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    std::array<VkClearValue, 1u> clear_values{color_clear_value};

    VkRenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.framebuffer = m_swapchain_framebuffers[i];
    render_pass_begin_info.renderPass = m_swapchain_render_pass;
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = clear_values.data();
    render_pass_begin_info.renderArea.extent = m_swapchain_extent;
    render_pass_begin_info.renderArea.offset = {.x{0}, .y{0}};
    render_pass_begin_info.pNext = nullptr;

    VkViewport viewport;
    viewport.width = static_cast<float>(m_swapchain_extent.width);
    viewport.height = static_cast<float>(m_swapchain_extent.height);
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.extent = m_swapchain_extent;
    scissor.offset = {.x{0}, .y{0}};

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    constexpr std::array<VkDeviceSize, 1ull> offsets{0};
    const VkBuffer vertex_buffer_handle{m_vertex_buffer->handle()};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_handle,
                           offsets.data());

    vkCmdBindIndexBuffer(command_buffer, m_index_buffer->handle(), 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_graphics_pipeline);

    const std::array<VkDescriptorSet, 2> descriptor_sets{
        m_graphics_pipeline_ubo_buffer_descriptor_set,
        m_graphics_pipeline_color_palette_descriptor_set};
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_graphics_pipeline_layout, 0,
                            static_cast<uint32_t>(descriptor_sets.size()),
                            descriptor_sets.data(), 0, nullptr);

    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
  }

  return true;
}

void realtime_mandelbrot_application::update_frame_data(
    const float delta_time) {
  static float zoom_scale = 1.0f;
  if (!m_window.has_value()) {
    return;
  }

  const auto [window_width, window_height] = m_window->get_size();

  if (window_width <= 0 || window_height <= 0)
    return;

  const float aspect_ratio =
      static_cast<float>(window_width) / static_cast<float>(window_height);
  static uniform_buffer_object ubo = {
      .aspect_ratio{aspect_ratio},
      .center_x{0.5f},
      .center_y{0.0f},
      .zoom_scale{zoom_scale},
      .iteration_count{800},
      .padding_x{},
      .padding_y{},
      .padding_z{},
  };

  constexpr float move_speed_factor = 0.25f;
  constexpr float zoom_speed_factor = 1.0f;

  const float zoom_speed = zoom_speed_factor;
  const float move_speed = m_input.is_key_pressed(key_code::shift)
                               ? move_speed_factor * 2.0f
                               : move_speed_factor;
  /* Move */
  if (m_input.is_key_pressed(key_code::z))
    zoom_scale += zoom_scale * zoom_speed * delta_time;

  if (m_input.is_key_pressed(key_code::x))
    zoom_scale -= zoom_scale * zoom_speed * delta_time;

  if (m_input.is_key_pressed(key_code::w))
    ubo.center_y += move_speed * delta_time * zoom_scale;

  if (m_input.is_key_pressed(key_code::s))
    ubo.center_y -= move_speed * delta_time * zoom_scale;

  if (m_input.is_key_pressed(key_code::a))
    ubo.center_x += move_speed * delta_time * zoom_scale;

  if (m_input.is_key_pressed(key_code::d))
    ubo.center_x -= move_speed * delta_time * zoom_scale;

  if (m_input.is_key_pressed(key_code::up))
    ubo.iteration_count += 1;

  if (m_input.is_key_pressed(key_code::down))
    ubo.iteration_count -= 1;

  /* Cap the zoom scale to avoid black border as we are rendering a quad */
  zoom_scale =
      zoom_scale > 1.0f * aspect_ratio ? 1.0f * aspect_ratio : fabs(zoom_scale);
  /* Update uniform buffer block */
  ubo.zoom_scale = zoom_scale;
  ubo.aspect_ratio = aspect_ratio;

  if (!m_ubo_buffer.has_value() ||
      !m_ubo_buffer->write(std::as_bytes(std::span{&ubo, 1}))) {
    return;
  }

  VkDescriptorBufferInfo buffer_info;
  buffer_info.buffer = m_ubo_buffer->handle();
  buffer_info.range = sizeof(uniform_buffer_object);
  buffer_info.offset = 0;

  VkWriteDescriptorSet ubo_buffer_descriptor_set_write{};
  ubo_buffer_descriptor_set_write.sType =
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  ubo_buffer_descriptor_set_write.descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_buffer_descriptor_set_write.dstBinding = 0;
  ubo_buffer_descriptor_set_write.dstArrayElement = 0;
  ubo_buffer_descriptor_set_write.descriptorCount = 1;
  ubo_buffer_descriptor_set_write.dstSet =
      m_graphics_pipeline_ubo_buffer_descriptor_set;
  ubo_buffer_descriptor_set_write.pBufferInfo = &buffer_info;
  ubo_buffer_descriptor_set_write.pImageInfo = nullptr;
  ubo_buffer_descriptor_set_write.pTexelBufferView = nullptr;
  ubo_buffer_descriptor_set_write.pNext = nullptr;
}

void realtime_mandelbrot_application::draw_frame() {
  VkResult result = vkAcquireNextImageKHR(
      m_context.device(), m_swapchain, max_swapchain_timeout,
      m_semaphores.present_complete[m_frame_index], VK_NULL_HANDLE,
      &m_image_index);

  if (result != VK_SUCCESS) {
    if (m_window.has_value()) {
      const auto [window_width, window_height] = m_window->get_size();
      recreate_swapchain(window_width, window_height);
    }
    return;
  }

  if (m_images_in_flight[m_image_index] != VK_NULL_HANDLE)
    vkWaitForFences(m_context.device(), 1, &m_images_in_flight[m_image_index],
                    VK_TRUE, UINT64_MAX);

  m_images_in_flight[m_image_index] = m_in_flight_fences[m_frame_index];

  const std::array<VkPipelineStageFlags, 1ull> wait_stages{
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers =
      &m_graphics_pipeline_command_buffers[m_image_index];
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &m_semaphores.present_complete[m_frame_index];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &m_semaphores.render_complete[m_frame_index];
  submit_info.pWaitDstStageMask = wait_stages.data();

  VK_CHECK(
      vkResetFences(m_context.device(), 1, &m_in_flight_fences[m_frame_index]));

  VK_CHECK(vkQueueSubmit(m_context.graphics_queue(), 1, &submit_info,
                         m_in_flight_fences[m_frame_index]));

  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &m_swapchain;
  present_info.pImageIndices = &m_image_index;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &m_semaphores.render_complete[m_frame_index];
  present_info.pResults = nullptr;
  present_info.pNext = nullptr;

  result = vkQueuePresentKHR(m_context.graphics_queue(), &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR && m_window.has_value()) {
    const auto [window_width, window_height] = m_window->get_size();
    recreate_swapchain(window_width, window_height);
  }

  m_frame_index = (m_frame_index + 1) % m_max_frames_in_flight;
}

void realtime_mandelbrot_application::recreate_swapchain(
    const uint32_t width, const uint32_t height) {
  m_swapchain_extent.width = width;
  m_swapchain_extent.height = height;

  if (!m_window.has_value()) {
    return;
  }

  while (m_swapchain_extent.width == 0 || m_swapchain_extent.height == 0) {
    m_window->poll([this](const event &polled) { on_event(polled); });
    const auto [window_width, window_height] = m_window->get_size();

    m_swapchain_extent.width = window_width;
    m_swapchain_extent.height = window_height;
  }

  VK_CHECK(vkDeviceWaitIdle(m_context.device()));
  cleanup_swapchain();
  create_swapchain();
  record_graphics_command_buffers();
}

void realtime_mandelbrot_application::cleanup_swapchain() {
  vkDestroyRenderPass(m_context.device(), m_swapchain_render_pass, nullptr);

  for (uint32_t i = 0; i < m_image_count; ++i) {
    vkDestroyFramebuffer(m_context.device(), m_swapchain_framebuffers[i],
                         nullptr);

    vkDestroyImageView(m_context.device(), m_swapchain_image_views[i], nullptr);

    vkDestroyFence(m_context.device(), m_in_flight_fences[i], nullptr);
  }

  m_images_in_flight.clear();
  for (uint32_t i = 0; i < m_max_frames_in_flight; ++i) {
    if (m_semaphores.present_complete.empty())
      return;

    if (m_semaphores.present_complete[i])
      vkDestroySemaphore(m_context.device(), m_semaphores.present_complete[i],
                         nullptr);

    if (m_semaphores.render_complete[i])
      vkDestroySemaphore(m_context.device(), m_semaphores.render_complete[i],
                         nullptr);
  }

  vkDestroySwapchainKHR(m_context.device(), m_swapchain, nullptr);
}
