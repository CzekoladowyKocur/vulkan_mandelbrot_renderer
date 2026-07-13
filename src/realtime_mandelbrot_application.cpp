#include "include/realtime_mandelbrot_application.hpp"
#include "include/input.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <print>
#include <span>
#include <string>
#include <string_view>

namespace {
constexpr std::uint64_t max_swapchain_timeout{UINT64_MAX};
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
  if (m_graphics_pipeline) {
    vkDestroyPipeline(m_context.device(), m_graphics_pipeline, nullptr);
  }

  if (m_graphics_pipeline_layout) {
    vkDestroyPipelineLayout(m_context.device(), m_graphics_pipeline_layout,
                            nullptr);
  }

  if (m_graphics_pipeline_ubo_buffer_descriptor_set_layout) {
    vkDestroyDescriptorSetLayout(
        m_context.device(),
        m_graphics_pipeline_ubo_buffer_descriptor_set_layout, nullptr);
  }

  if (m_graphics_pipeline_color_palette_descriptor_set_layout) {
    vkDestroyDescriptorSetLayout(
        m_context.device(),
        m_graphics_pipeline_color_palette_descriptor_set_layout, nullptr);
  }

  if (m_graphics_pipeline_color_palette_descriptor_set) {
    vkFreeDescriptorSets(m_context.device(),
                         m_graphics_pipeline_descriptor_pool, 1,
                         &m_graphics_pipeline_color_palette_descriptor_set);
  }

  if (m_graphics_pipeline_descriptor_pool) {
    vkDestroyDescriptorPool(m_context.device(),
                            m_graphics_pipeline_descriptor_pool, nullptr);
  }

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

  VkBool32 supported{VK_FALSE};
  vkGetPhysicalDeviceSurfaceSupportKHR(m_context.physical_device(),
                                       m_context.graphics_queue_family(),
                                       m_surface, &supported);
  return true;
}

bool realtime_mandelbrot_application::create_swapchain() {
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      m_context.physical_device(), m_surface, &m_surface_capabilities));

  std::uint32_t surface_format_count{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_context.physical_device(), m_surface, &surface_format_count, nullptr));
  assert(surface_format_count > 0);

  std::vector<VkSurfaceFormatKHR> available_surface_formats(
      surface_format_count);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_context.physical_device(), m_surface, &surface_format_count,
      available_surface_formats.data()));

  m_surface_format = available_surface_formats[0];
  for (const VkSurfaceFormatKHR surface_format : available_surface_formats) {
    if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      m_surface_format = surface_format;
      break;
    }
  }

  std::uint32_t present_mode_count{};
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_context.physical_device(), m_surface, &present_mode_count, nullptr));
  assert(present_mode_count > 0);

  std::vector<VkPresentModeKHR> available_present_modes(present_mode_count);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_context.physical_device(), m_surface, &present_mode_count,
      available_present_modes.data()));

  /* The only present mode guaranteed to be supported by the specification */
  m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (const VkPresentModeKHR present_mode : available_present_modes) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      m_present_mode = present_mode;
      break;
    }
  }

  m_max_frames_in_flight =
      m_present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? (3) : (2);

  const VkExtent2D min_swapchain_image_extent{
      m_surface_capabilities.minImageExtent};
  const VkExtent2D max_swapchain_image_extent{
      m_surface_capabilities.maxImageExtent};

  /* Clamp */
  m_swapchain_extent.width =
      std::clamp(m_swapchain_extent.width, min_swapchain_image_extent.width,
                 max_swapchain_image_extent.width);
  m_swapchain_extent.height =
      std::clamp(m_swapchain_extent.height, min_swapchain_image_extent.height,
                 max_swapchain_image_extent.height);

  const std::uint32_t min_image_count{m_surface_capabilities.minImageCount};
  const std::uint32_t max_image_count{m_surface_capabilities.maxImageCount};
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

  const std::array<VkAttachmentDescription, 1uz> attachments{color_attachment};
  const VkRenderPassCreateInfo render_pass_create_info{
      .sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .attachmentCount{static_cast<std::uint32_t>(attachments.size())},
      .pAttachments{attachments.data()},
      .subpassCount{1u},
      .pSubpasses{&subpass_description},
      .dependencyCount{1u},
      .pDependencies{&subpass_dependency}};

  VK_CHECK(vkCreateRenderPass(m_context.device(), &render_pass_create_info,
                              nullptr, &m_swapchain_render_pass));

  std::uint32_t image_index{0u};
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
  for (std::uint32_t i{0u}; i < m_max_frames_in_flight; ++i) {
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
  constexpr VkDeviceSize vertex_buffer_size{sizeof(float) * 4 * 3};
  constexpr VkDeviceSize index_buffer_size{sizeof(std::uint32_t) * 6};

  const std::array<float, 4uz * 3uz> fullscreen_quad_vertices

      {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
       1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};

  const std::array<std::uint32_t, 6uz> fullscreen_quad_indices{0, 1, 2,
                                                               2, 3, 0};

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
  const bool device_supports_double_precision_floats{false};
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

  const VkPipelineShaderStageCreateInfo vertex_shader_stage_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .stage{VK_SHADER_STAGE_VERTEX_BIT},
      .module{m_vertex_shader_module},
      .pName{"main"},
      .pSpecializationInfo{nullptr}};

  const VkPipelineShaderStageCreateInfo fragment_shader_stage_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .stage{VK_SHADER_STAGE_FRAGMENT_BIT},
      .module{m_fragment_shader_module},
      .pName{"main"},
      .pSpecializationInfo{nullptr}};

  const std::array<VkPipelineShaderStageCreateInfo, 2uz> shader_stages{
      vertex_shader_stage_info, fragment_shader_stage_info};

  const VkVertexInputBindingDescription vertex_input_binding_description{
      .binding{},
      .stride{sizeof(float) * 3},
      .inputRate{VK_VERTEX_INPUT_RATE_VERTEX}};

  const VkVertexInputAttributeDescription vertex_input_attribute_description{
      .location{}, .binding{}, .format{VK_FORMAT_R32G32B32_SFLOAT}, .offset{}};

  const VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .vertexBindingDescriptionCount{1u},
      .pVertexBindingDescriptions{&vertex_input_binding_description},
      .vertexAttributeDescriptionCount{1u},
      .pVertexAttributeDescriptions{&vertex_input_attribute_description}};

  const VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
      .primitiveRestartEnable{VK_FALSE}};

  const VkViewport viewport{
      .x{0.0f},
      .y{0.0f},
      .width{static_cast<float>(m_swapchain_extent.width)},
      .height{static_cast<float>(m_swapchain_extent.height)},
      .minDepth{0.0f},
      .maxDepth{1.0f}};

  const VkRect2D scissor{.offset{.x{0}, .y{0}}, .extent{m_swapchain_extent}};

  const VkPipelineViewportStateCreateInfo viewport_state{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .viewportCount{1u},
      .pViewports{&viewport},
      .scissorCount{1u},
      .pScissors{&scissor}};

  const std::array<VkDynamicState, 2uz> dynamic_states{
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamic_state_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .dynamicStateCount{static_cast<std::uint32_t>(dynamic_states.size())},
      .pDynamicStates{dynamic_states.data()}};

  const VkPipelineRasterizationStateCreateInfo rasterizer_state_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .depthClampEnable{VK_FALSE},
      .rasterizerDiscardEnable{VK_FALSE},
      .polygonMode{VK_POLYGON_MODE_FILL},
      .cullMode{VK_CULL_MODE_BACK_BIT},
      .frontFace{VK_FRONT_FACE_CLOCKWISE},
      .depthBiasEnable{VK_FALSE},
      .depthBiasConstantFactor{},
      .depthBiasClamp{},
      .depthBiasSlopeFactor{},
      .lineWidth{1.0f}};

  const VkPipelineMultisampleStateCreateInfo multisampling{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT},
      .sampleShadingEnable{VK_FALSE},
      .minSampleShading{},
      .pSampleMask{nullptr},
      .alphaToCoverageEnable{VK_FALSE},
      .alphaToOneEnable{VK_FALSE}};

  const VkPipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable{VK_FALSE},
      .srcColorBlendFactor{},
      .dstColorBlendFactor{},
      .colorBlendOp{},
      .srcAlphaBlendFactor{},
      .dstAlphaBlendFactor{},
      .alphaBlendOp{},
      .colorWriteMask{VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

  const VkPipelineColorBlendStateCreateInfo color_blending{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .logicOpEnable{VK_FALSE},
      .logicOp{VK_LOGIC_OP_COPY},
      .attachmentCount{1u},
      .pAttachments{&color_blend_attachment},
      .blendConstants{0.0f, 0.0f, 0.0f, 0.0f}};

  {
    const VkDescriptorSetLayoutBinding ubo_binding{
        .binding{},
        .descriptorType{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        .descriptorCount{1u},
        .stageFlags{VK_SHADER_STAGE_VERTEX_BIT},
        .pImmutableSamplers{nullptr}};

    const std::array<VkDescriptorSetLayoutBinding, 1uz> bindings{ubo_binding};
    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .bindingCount{static_cast<std::uint32_t>(bindings.size())},
        .pBindings{bindings.data()}};

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_context.device(), &descriptor_set_layout_create_info, nullptr,
        &m_graphics_pipeline_ubo_buffer_descriptor_set_layout));
  }

  {
    const VkDescriptorSetLayoutBinding color_palette_binding{
        .binding{},
        .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        .descriptorCount{1u},
        .stageFlags{VK_SHADER_STAGE_FRAGMENT_BIT},
        .pImmutableSamplers{nullptr}};

    const std::array<VkDescriptorSetLayoutBinding, 1uz> bindings{
        color_palette_binding};
    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .bindingCount{static_cast<std::uint32_t>(bindings.size())},
        .pBindings{bindings.data()}};

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_context.device(), &descriptor_set_layout_create_info, nullptr,
        &m_graphics_pipeline_color_palette_descriptor_set_layout));
  }

  const std::array<VkDescriptorSetLayout, 2uz> descriptor_set_layouts{
      m_graphics_pipeline_ubo_buffer_descriptor_set_layout,
      m_graphics_pipeline_color_palette_descriptor_set_layout};
  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .setLayoutCount{
          static_cast<std::uint32_t>(descriptor_set_layouts.size())},
      .pSetLayouts{descriptor_set_layouts.data()},
      .pushConstantRangeCount{},
      .pPushConstantRanges{nullptr}};

  if (!m_color_palette_texture.has_value()) {
    return false;
  }

  const VkDescriptorImageInfo image_info{
      .sampler{m_color_palette_texture->sampler()},
      .imageView{m_color_palette_texture->image_view()},
      .imageLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

  const VkDescriptorPoolSize ubo_buffer_descriptor_pool_size{
      .type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}, .descriptorCount{1u}};

  const VkDescriptorPoolSize color_palette_image_descriptor_pool_size{
      .type{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, .descriptorCount{1u}};

  const std::array<VkDescriptorPoolSize, 2uz> descriptor_pool_sizes{
      ubo_buffer_descriptor_pool_size,
      color_palette_image_descriptor_pool_size};
  const VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
      .pNext{nullptr},
      .flags{VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT},
      .maxSets{10u},
      .poolSizeCount{static_cast<std::uint32_t>(descriptor_pool_sizes.size())},
      .pPoolSizes{descriptor_pool_sizes.data()}};

  VK_CHECK(vkCreateDescriptorPool(m_context.device(),
                                  &descriptor_pool_create_info, nullptr,
                                  &m_graphics_pipeline_descriptor_pool));

  const VkDescriptorSetAllocateInfo ubo_buffer_descriptor_set_allocate_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
      .pNext{nullptr},
      .descriptorPool{m_graphics_pipeline_descriptor_pool},
      .descriptorSetCount{1u},
      .pSetLayouts{&m_graphics_pipeline_ubo_buffer_descriptor_set_layout}};

  VK_CHECK(vkAllocateDescriptorSets(
      m_context.device(), &ubo_buffer_descriptor_set_allocate_info,
      &m_graphics_pipeline_ubo_buffer_descriptor_set));

  const VkDescriptorSetAllocateInfo
      color_palette_image_descriptor_set_allocate_info{
          .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
          .pNext{nullptr},
          .descriptorPool{m_graphics_pipeline_descriptor_pool},
          .descriptorSetCount{1u},
          .pSetLayouts{
              &m_graphics_pipeline_color_palette_descriptor_set_layout}};

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

  const VkWriteDescriptorSet color_palette_descriptor_set_write{
      .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
      .pNext{nullptr},
      .dstSet{m_graphics_pipeline_color_palette_descriptor_set},
      .dstBinding{},
      .dstArrayElement{},
      .descriptorCount{1u},
      .descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
      .pImageInfo{&image_info},
      .pBufferInfo{nullptr},
      .pTexelBufferView{nullptr}};

  vkUpdateDescriptorSets(m_context.device(), 1,
                         &color_palette_descriptor_set_write, 0, nullptr);

  const auto temporary{glm::mat4{1.0f}};
  if (!m_ubo_buffer->write(std::as_bytes(std::span{&temporary, 1})
                               .first(sizeof(uniform_buffer_object)))) {
    return false;
  }

  const VkDescriptorBufferInfo buffer_info{
      .buffer{m_ubo_buffer->handle()},
      .offset{},
      .range{sizeof(uniform_buffer_object)}};

  const VkWriteDescriptorSet ubo_buffer_descriptor_set_write{
      .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
      .pNext{nullptr},
      .dstSet{m_graphics_pipeline_ubo_buffer_descriptor_set},
      .dstBinding{},
      .dstArrayElement{},
      .descriptorCount{1u},
      .descriptorType{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
      .pImageInfo{nullptr},
      .pBufferInfo{&buffer_info},
      .pTexelBufferView{nullptr}};

  vkUpdateDescriptorSets(m_context.device(), 1,
                         &ubo_buffer_descriptor_set_write, 0, nullptr);

  if (vkCreatePipelineLayout(m_context.device(), &pipeline_layout_info, nullptr,
                             &m_graphics_pipeline_layout) != VK_SUCCESS) {
    std::println("Failed to create graphics pipeline layout");
    return false;
  }

  const VkGraphicsPipelineCreateInfo pipeline_info{
      .sType{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .stageCount{static_cast<std::uint32_t>(shader_stages.size())},
      .pStages{shader_stages.data()},
      .pVertexInputState{&vertex_input_info},
      .pInputAssemblyState{&input_assembly},
      .pTessellationState{nullptr},
      .pViewportState{&viewport_state},
      .pRasterizationState{&rasterizer_state_info},
      .pMultisampleState{&multisampling},
      .pDepthStencilState{nullptr},
      .pColorBlendState{&color_blending},
      .pDynamicState{&dynamic_state_info},
      .layout{m_graphics_pipeline_layout},
      .renderPass{m_swapchain_render_pass},
      .subpass{},
      .basePipelineHandle{VK_NULL_HANDLE},
      .basePipelineIndex{}};

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
  const VkCommandBufferAllocateInfo command_buffer_allocate_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
      .pNext{nullptr},
      .commandPool{m_context.graphics_command_pool()},
      .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
      .commandBufferCount{m_image_count}};

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

  for (std::uint32_t i{0u}; i < m_image_count; ++i) {
    VkCommandBuffer &command_buffer{m_graphics_pipeline_command_buffers[i]};
    const VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
        .pNext{nullptr},
        .flags{},
        .pInheritanceInfo{nullptr}};

    const VkClearValue color_clear_value{{{0.0f, 0.0f, 0.0f, 1.0f}}};
    const std::array<VkClearValue, 1uz> clear_values{color_clear_value};

    const VkRenderPassBeginInfo render_pass_begin_info{
        .sType{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO},
        .pNext{nullptr},
        .renderPass{m_swapchain_render_pass},
        .framebuffer{m_swapchain_framebuffers[i]},
        .renderArea{.offset{.x{0}, .y{0}}, .extent{m_swapchain_extent}},
        .clearValueCount{1u},
        .pClearValues{clear_values.data()}};

    const VkViewport viewport{
        .x{0.0f},
        .y{0.0f},
        .width{static_cast<float>(m_swapchain_extent.width)},
        .height{static_cast<float>(m_swapchain_extent.height)},
        .minDepth{0.0f},
        .maxDepth{1.0f}};

    const VkRect2D scissor{.offset{.x{0}, .y{0}}, .extent{m_swapchain_extent}};

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    constexpr std::array<VkDeviceSize, 1uz> offsets{0};
    const VkBuffer vertex_buffer_handle{m_vertex_buffer->handle()};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_handle,
                           offsets.data());

    vkCmdBindIndexBuffer(command_buffer, m_index_buffer->handle(), 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_graphics_pipeline);

    const std::array<VkDescriptorSet, 2uz> descriptor_sets{
        m_graphics_pipeline_ubo_buffer_descriptor_set,
        m_graphics_pipeline_color_palette_descriptor_set};
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_graphics_pipeline_layout, 0,
                            static_cast<std::uint32_t>(descriptor_sets.size()),
                            descriptor_sets.data(), 0, nullptr);

    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
  }

  return true;
}

void realtime_mandelbrot_application::update_frame_data(
    const float delta_time) {
  static float zoom_scale{1.0f};
  if (!m_window.has_value()) {
    return;
  }

  const auto [window_width, window_height]{m_window->get_size()};

  if (window_width <= 0 || window_height <= 0) {
    return;
  }

  const float aspect_ratio{static_cast<float>(window_width) /
                           static_cast<float>(window_height)};
  static uniform_buffer_object ubo{
      .aspect_ratio{aspect_ratio},
      .center_x{0.5f},
      .center_y{0.0f},
      .zoom_scale{zoom_scale},
      .iteration_count{800},
      .padding_x{},
      .padding_y{},
      .padding_z{},
  };

  constexpr float move_speed_factor{0.25f};
  constexpr float zoom_speed_factor{1.0f};

  const float zoom_speed{zoom_speed_factor};
  const float move_speed{m_input.is_key_pressed(key_code::shift)
                             ? move_speed_factor * 2.0f
                             : move_speed_factor};
  /* Move */
  if (m_input.is_key_pressed(key_code::z)) {
    zoom_scale += zoom_scale * zoom_speed * delta_time;
  }

  if (m_input.is_key_pressed(key_code::x)) {
    zoom_scale -= zoom_scale * zoom_speed * delta_time;
  }

  if (m_input.is_key_pressed(key_code::w)) {
    ubo.center_y += move_speed * delta_time * zoom_scale;
  }

  if (m_input.is_key_pressed(key_code::s)) {
    ubo.center_y -= move_speed * delta_time * zoom_scale;
  }

  if (m_input.is_key_pressed(key_code::a)) {
    ubo.center_x += move_speed * delta_time * zoom_scale;
  }

  if (m_input.is_key_pressed(key_code::d)) {
    ubo.center_x -= move_speed * delta_time * zoom_scale;
  }

  if (m_input.is_key_pressed(key_code::up)) {
    ubo.iteration_count += 1;
  }

  if (m_input.is_key_pressed(key_code::down)) {
    ubo.iteration_count -= 1;
  }

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
}

void realtime_mandelbrot_application::draw_frame() {
  VkResult result{vkAcquireNextImageKHR(
      m_context.device(), m_swapchain, max_swapchain_timeout,
      m_semaphores.present_complete[m_frame_index], VK_NULL_HANDLE,
      &m_image_index)};

  if (result != VK_SUCCESS) {
    if (m_window.has_value()) {
      const auto [window_width, window_height]{m_window->get_size()};
      recreate_swapchain(window_width, window_height);
    }
    return;
  }

  if (m_images_in_flight[m_image_index] != VK_NULL_HANDLE) {
    vkWaitForFences(m_context.device(), 1, &m_images_in_flight[m_image_index],
                    VK_TRUE, UINT64_MAX);
  }

  m_images_in_flight[m_image_index] = m_in_flight_fences[m_frame_index];

  const std::array<VkPipelineStageFlags, 1uz> wait_stages{
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  const VkSubmitInfo submit_info{
      .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
      .pNext{nullptr},
      .waitSemaphoreCount{1u},
      .pWaitSemaphores{&m_semaphores.present_complete[m_frame_index]},
      .pWaitDstStageMask{wait_stages.data()},
      .commandBufferCount{1u},
      .pCommandBuffers{&m_graphics_pipeline_command_buffers[m_image_index]},
      .signalSemaphoreCount{1u},
      .pSignalSemaphores{&m_semaphores.render_complete[m_frame_index]}};

  VK_CHECK(
      vkResetFences(m_context.device(), 1, &m_in_flight_fences[m_frame_index]));

  VK_CHECK(vkQueueSubmit(m_context.graphics_queue(), 1, &submit_info,
                         m_in_flight_fences[m_frame_index]));

  const VkPresentInfoKHR present_info{
      .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
      .pNext{nullptr},
      .waitSemaphoreCount{1u},
      .pWaitSemaphores{&m_semaphores.render_complete[m_frame_index]},
      .swapchainCount{1u},
      .pSwapchains{&m_swapchain},
      .pImageIndices{&m_image_index},
      .pResults{nullptr}};

  result = vkQueuePresentKHR(m_context.graphics_queue(), &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR && m_window.has_value()) {
    const auto [window_width, window_height]{m_window->get_size()};
    recreate_swapchain(window_width, window_height);
  }

  m_frame_index = (m_frame_index + 1) % m_max_frames_in_flight;
}

void realtime_mandelbrot_application::recreate_swapchain(
    const std::uint32_t width, const std::uint32_t height) {
  m_swapchain_extent.width = width;
  m_swapchain_extent.height = height;

  if (!m_window.has_value()) {
    return;
  }

  while (m_swapchain_extent.width == 0 || m_swapchain_extent.height == 0) {
    m_window->poll([this](const event &polled) { on_event(polled); });
    const auto [window_width, window_height]{m_window->get_size()};

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

  for (std::uint32_t i{0u}; i < m_image_count; ++i) {
    vkDestroyFramebuffer(m_context.device(), m_swapchain_framebuffers[i],
                         nullptr);

    vkDestroyImageView(m_context.device(), m_swapchain_image_views[i], nullptr);

    vkDestroyFence(m_context.device(), m_in_flight_fences[i], nullptr);
  }

  m_images_in_flight.clear();
  for (std::uint32_t i{0u}; i < m_max_frames_in_flight; ++i) {
    if (m_semaphores.present_complete.empty()) {
      return;
    }

    if (m_semaphores.present_complete[i]) {
      vkDestroySemaphore(m_context.device(), m_semaphores.present_complete[i],
                         nullptr);
    }

    if (m_semaphores.render_complete[i]) {
      vkDestroySemaphore(m_context.device(), m_semaphores.render_complete[i],
                         nullptr);
    }
  }

  vkDestroySwapchainKHR(m_context.device(), m_swapchain, nullptr);
}
