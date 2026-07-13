#include "include/realtime_mandelbrot_application.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {

constexpr std::uint64_t max_swapchain_timeout{UINT64_MAX};

[[nodiscard]] std::expected<vulkan_buffer, std::error_code>
create_device_local_buffer(const vulkan_context &context,
                           const std::span<const std::byte> data,
                           const VkBufferUsageFlags usage) {
  auto staging{vulkan_buffer::create(
      {.size{data.size()},
       .device{context.device()},
       .physical_device{context.physical_device()},
       .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
  if (!staging) {
    return staging;
  }

  if (const auto written{staging->write(data)}; !written) {
    return std::unexpected{written.error()};
  }

  auto buffer{vulkan_buffer::create(
      {.size{data.size()},
       .device{context.device()},
       .physical_device{context.physical_device()},
       .usage{usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}})};
  if (!buffer) {
    return buffer;
  }

  const single_time_command_context upload_context{
      .device{context.device()},
      .command_pool{context.graphics_command_pool()},
      .queue{context.graphics_queue()}};

  if (const auto copied{copy_buffer(upload_context, staging->handle(),
                                    buffer->handle(), data.size())};
      !copied) {
    return std::unexpected{copied.error()};
  }

  return buffer;
}

} // namespace

realtime_mandelbrot_application::surface_resource::surface_resource(
    surface_resource &&other) noexcept
    : instance{other.instance}, handle{other.handle} {
  other.instance = VK_NULL_HANDLE;
  other.handle = VK_NULL_HANDLE;
}

realtime_mandelbrot_application::surface_resource &
realtime_mandelbrot_application::surface_resource::operator=(
    surface_resource &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  if (handle != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, handle, nullptr);
  }

  instance = other.instance;
  handle = other.handle;
  other.instance = VK_NULL_HANDLE;
  other.handle = VK_NULL_HANDLE;
  return *this;
}

realtime_mandelbrot_application::surface_resource::~surface_resource() {
  if (handle != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, handle, nullptr);
  }
}

realtime_mandelbrot_application::swapchain_resources::swapchain_resources(
    swapchain_resources &&other) noexcept
    : device{other.device}, swapchain{other.swapchain},
      surface_format{other.surface_format}, extent{other.extent},
      image_count{other.image_count},
      max_frames_in_flight{other.max_frames_in_flight},
      render_pass{other.render_pass}, images{std::move(other.images)},
      image_views{std::move(other.image_views)},
      framebuffers{std::move(other.framebuffers)},
      present_complete{std::move(other.present_complete)},
      render_complete{std::move(other.render_complete)},
      in_flight_fences{std::move(other.in_flight_fences)},
      images_in_flight{std::move(other.images_in_flight)} {
  other.device = VK_NULL_HANDLE;
  other.swapchain = VK_NULL_HANDLE;
  other.render_pass = VK_NULL_HANDLE;
}

realtime_mandelbrot_application::swapchain_resources &
realtime_mandelbrot_application::swapchain_resources::operator=(
    swapchain_resources &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  device = other.device;
  swapchain = other.swapchain;
  surface_format = other.surface_format;
  extent = other.extent;
  image_count = other.image_count;
  max_frames_in_flight = other.max_frames_in_flight;
  render_pass = other.render_pass;
  images = std::move(other.images);
  image_views = std::move(other.image_views);
  framebuffers = std::move(other.framebuffers);
  present_complete = std::move(other.present_complete);
  render_complete = std::move(other.render_complete);
  in_flight_fences = std::move(other.in_flight_fences);
  images_in_flight = std::move(other.images_in_flight);

  other.device = VK_NULL_HANDLE;
  other.swapchain = VK_NULL_HANDLE;
  other.render_pass = VK_NULL_HANDLE;
  return *this;
}

realtime_mandelbrot_application::swapchain_resources::~swapchain_resources() {
  destroy();
}

void realtime_mandelbrot_application::swapchain_resources::destroy() noexcept {
  if (device == VK_NULL_HANDLE) {
    return;
  }

  vkDestroyRenderPass(device, render_pass, nullptr);

  for (const VkFramebuffer framebuffer : framebuffers) {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }

  for (const VkImageView image_view : image_views) {
    vkDestroyImageView(device, image_view, nullptr);
  }

  for (const VkFence fence : in_flight_fences) {
    vkDestroyFence(device, fence, nullptr);
  }

  for (const VkSemaphore semaphore : present_complete) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }

  for (const VkSemaphore semaphore : render_complete) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }

  vkDestroySwapchainKHR(device, swapchain, nullptr);
}

realtime_mandelbrot_application::pipeline_resources::pipeline_resources(
    pipeline_resources &&other) noexcept
    : device{other.device},
      ubo_descriptor_set_layout{other.ubo_descriptor_set_layout},
      descriptor_pool{other.descriptor_pool},
      ubo_descriptor_set{other.ubo_descriptor_set},
      pipeline_layout{other.pipeline_layout}, pipeline{other.pipeline} {
  other.device = VK_NULL_HANDLE;
  other.ubo_descriptor_set_layout = VK_NULL_HANDLE;
  other.descriptor_pool = VK_NULL_HANDLE;
  other.ubo_descriptor_set = VK_NULL_HANDLE;
  other.pipeline_layout = VK_NULL_HANDLE;
  other.pipeline = VK_NULL_HANDLE;
}

realtime_mandelbrot_application::pipeline_resources &
realtime_mandelbrot_application::pipeline_resources::operator=(
    pipeline_resources &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  device = other.device;
  ubo_descriptor_set_layout = other.ubo_descriptor_set_layout;
  descriptor_pool = other.descriptor_pool;
  ubo_descriptor_set = other.ubo_descriptor_set;
  pipeline_layout = other.pipeline_layout;
  pipeline = other.pipeline;

  other.device = VK_NULL_HANDLE;
  other.ubo_descriptor_set_layout = VK_NULL_HANDLE;
  other.descriptor_pool = VK_NULL_HANDLE;
  other.ubo_descriptor_set = VK_NULL_HANDLE;
  other.pipeline_layout = VK_NULL_HANDLE;
  other.pipeline = VK_NULL_HANDLE;
  return *this;
}

realtime_mandelbrot_application::pipeline_resources::~pipeline_resources() {
  destroy();
}

void realtime_mandelbrot_application::pipeline_resources::destroy() noexcept {
  if (device == VK_NULL_HANDLE) {
    return;
  }

  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
  vkDestroyDescriptorSetLayout(device, ubo_descriptor_set_layout, nullptr);
}

realtime_mandelbrot_application::graphics_command_buffers::
    graphics_command_buffers(graphics_command_buffers &&other) noexcept
    : device{other.device}, command_pool{other.command_pool},
      buffers{std::move(other.buffers)} {
  other.device = VK_NULL_HANDLE;
  other.command_pool = VK_NULL_HANDLE;
}

realtime_mandelbrot_application::graphics_command_buffers &
realtime_mandelbrot_application::graphics_command_buffers::operator=(
    graphics_command_buffers &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  device = other.device;
  command_pool = other.command_pool;
  buffers = std::move(other.buffers);

  other.device = VK_NULL_HANDLE;
  other.command_pool = VK_NULL_HANDLE;
  return *this;
}

realtime_mandelbrot_application::graphics_command_buffers::
    ~graphics_command_buffers() {
  destroy();
}

void realtime_mandelbrot_application::graphics_command_buffers::
    destroy() noexcept {
  if (device == VK_NULL_HANDLE || buffers.empty()) {
    return;
  }

  vkFreeCommandBuffers(device, command_pool,
                       static_cast<std::uint32_t>(buffers.size()),
                       buffers.data());
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

std::expected<realtime_mandelbrot_application::surface_resource,
              std::error_code>
realtime_mandelbrot_application::create_surface(const vulkan_context &context,
                                                window &target_window) {
  const auto created{target_window.get_surface(context.instance())};
  if (!created) {
    return std::unexpected{created.error()};
  }

  surface_resource surface{};
  surface.instance = context.instance();
  surface.handle = *created;

  VkBool32 supported{VK_FALSE};
  if (const VkResult result{vkGetPhysicalDeviceSurfaceSupportKHR(
          context.physical_device(), context.graphics_queue_family(),
          surface.handle, &supported)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (supported != VK_TRUE) {
    return std::unexpected{
        std::make_error_code(std::errc::operation_not_supported)};
  }

  return surface;
}

std::expected<realtime_mandelbrot_application::swapchain_resources,
              std::error_code>
realtime_mandelbrot_application::create_swapchain_resources(
    const vulkan_context &context, const VkSurfaceKHR surface) {
  VkSurfaceCapabilitiesKHR surface_capabilities{
      .currentTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR}};
  if (const VkResult result{vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          context.physical_device(), surface, &surface_capabilities)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::uint32_t surface_format_count{};
  if (const VkResult result{vkGetPhysicalDeviceSurfaceFormatsKHR(
          context.physical_device(), surface, &surface_format_count, nullptr)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::vector<VkSurfaceFormatKHR> available_surface_formats(
      surface_format_count);
  if (const VkResult result{vkGetPhysicalDeviceSurfaceFormatsKHR(
          context.physical_device(), surface, &surface_format_count,
          available_surface_formats.data())};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (available_surface_formats.empty()) {
    return std::unexpected{
        std::make_error_code(std::errc::operation_not_supported)};
  }

  swapchain_resources resources{};
  resources.device = context.device();

  resources.surface_format = available_surface_formats[0];
  for (const VkSurfaceFormatKHR surface_format : available_surface_formats) {
    if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      resources.surface_format = surface_format;
      break;
    }
  }

  std::uint32_t present_mode_count{};
  if (const VkResult result{vkGetPhysicalDeviceSurfacePresentModesKHR(
          context.physical_device(), surface, &present_mode_count, nullptr)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::vector<VkPresentModeKHR> available_present_modes(present_mode_count);
  if (const VkResult result{vkGetPhysicalDeviceSurfacePresentModesKHR(
          context.physical_device(), surface, &present_mode_count,
          available_present_modes.data())};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  /* The only present mode guaranteed to be supported by the specification */
  VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
  for (const VkPresentModeKHR available_mode : available_present_modes) {
    if (available_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = available_mode;
      break;
    }
  }

  resources.max_frames_in_flight =
      present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? 3u : 2u;

  m_swapchain_extent.width = std::clamp(
      m_swapchain_extent.width, surface_capabilities.minImageExtent.width,
      surface_capabilities.maxImageExtent.width);
  m_swapchain_extent.height = std::clamp(
      m_swapchain_extent.height, surface_capabilities.minImageExtent.height,
      surface_capabilities.maxImageExtent.height);
  resources.extent = m_swapchain_extent;

  const std::uint32_t min_image_count{surface_capabilities.minImageCount};
  const std::uint32_t max_image_count{surface_capabilities.maxImageCount};
  resources.image_count = (min_image_count + 1u) < max_image_count
                              ? (min_image_count + 1u)
                              : max_image_count;

  const VkSwapchainCreateInfoKHR swapchain_create_info{
      .sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
      .pNext{nullptr},
      .flags{},
      .surface{surface},
      .minImageCount{resources.image_count},
      .imageFormat{resources.surface_format.format},
      .imageColorSpace{resources.surface_format.colorSpace},
      .imageExtent{resources.extent},
      .imageArrayLayers{1u},
      .imageUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
      .imageSharingMode{VK_SHARING_MODE_EXCLUSIVE},
      .queueFamilyIndexCount{},
      .pQueueFamilyIndices{nullptr},
      .preTransform{surface_capabilities.currentTransform},
      .compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
      .presentMode{present_mode},
      .clipped{VK_TRUE},
      .oldSwapchain{VK_NULL_HANDLE}};

  if (const VkResult result{
          vkCreateSwapchainKHR(context.device(), &swapchain_create_info,
                               nullptr, &resources.swapchain)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  resources.image_count = 0u;
  if (const VkResult result{
          vkGetSwapchainImagesKHR(context.device(), resources.swapchain,
                                  &resources.image_count, nullptr)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  resources.images.resize(resources.image_count);
  resources.image_views.resize(resources.image_count);
  resources.framebuffers.resize(resources.image_count);

  if (const VkResult result{vkGetSwapchainImagesKHR(
          context.device(), resources.swapchain, &resources.image_count,
          resources.images.data())};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkAttachmentDescription color_attachment{
      .flags{},
      .format{resources.surface_format.format},
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

  const VkRenderPassCreateInfo render_pass_create_info{
      .sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .attachmentCount{1u},
      .pAttachments{&color_attachment},
      .subpassCount{1u},
      .pSubpasses{&subpass_description},
      .dependencyCount{1u},
      .pDependencies{&subpass_dependency}};

  if (const VkResult result{
          vkCreateRenderPass(context.device(), &render_pass_create_info,
                             nullptr, &resources.render_pass)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  for (std::uint32_t image_index{0u}; image_index < resources.image_count;
       ++image_index) {
    const VkImageViewCreateInfo image_view_create_info{
        .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .image{resources.images[image_index]},
        .viewType{VK_IMAGE_VIEW_TYPE_2D},
        .format{resources.surface_format.format},
        .components{.r{VK_COMPONENT_SWIZZLE_R},
                    .g{VK_COMPONENT_SWIZZLE_G},
                    .b{VK_COMPONENT_SWIZZLE_B},
                    .a{VK_COMPONENT_SWIZZLE_A}},
        .subresourceRange{.aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                          .baseMipLevel{},
                          .levelCount{1u},
                          .baseArrayLayer{},
                          .layerCount{1u}}};

    if (const VkResult result{
            vkCreateImageView(context.device(), &image_view_create_info,
                              nullptr, &resources.image_views[image_index])};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    const VkFramebufferCreateInfo framebuffer_create_info{
        .sType{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .renderPass{resources.render_pass},
        .attachmentCount{1u},
        .pAttachments{&resources.image_views[image_index]},
        .width{resources.extent.width},
        .height{resources.extent.height},
        .layers{1u}};

    if (const VkResult result{
            vkCreateFramebuffer(context.device(), &framebuffer_create_info,
                                nullptr, &resources.framebuffers[image_index])};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  const VkSemaphoreCreateInfo semaphore_create_info{
      .sType{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO},
      .pNext{nullptr},
      .flags{}};

  const VkFenceCreateInfo fence_create_info{
      .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO},
      .pNext{nullptr},
      .flags{VK_FENCE_CREATE_SIGNALED_BIT}};

  resources.present_complete.resize(resources.max_frames_in_flight,
                                    VK_NULL_HANDLE);
  resources.render_complete.resize(resources.image_count, VK_NULL_HANDLE);
  resources.in_flight_fences.resize(resources.max_frames_in_flight,
                                    VK_NULL_HANDLE);
  for (std::uint32_t i{0u}; i < resources.max_frames_in_flight; ++i) {
    if (const VkResult result{
            vkCreateSemaphore(context.device(), &semaphore_create_info, nullptr,
                              &resources.present_complete[i])};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    if (const VkResult result{vkCreateFence(context.device(),
                                            &fence_create_info, nullptr,
                                            &resources.in_flight_fences[i])};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  for (std::uint32_t i{0u}; i < resources.image_count; ++i) {
    if (const VkResult result{vkCreateSemaphore(context.device(),
                                                &semaphore_create_info, nullptr,
                                                &resources.render_complete[i])};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  resources.images_in_flight.resize(resources.image_count, VK_NULL_HANDLE);
  return resources;
}

std::expected<realtime_mandelbrot_application::pipeline_resources,
              std::error_code>
realtime_mandelbrot_application::create_pipeline_resources(
    const vulkan_context &context, const swapchain_resources &swapchain,
    const vulkan_buffer &ubo_buffer) {
  pipeline_resources resources{};
  resources.device = context.device();

  {
    const VkDescriptorSetLayoutBinding ubo_binding{
        .binding{},
        .descriptorType{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        .descriptorCount{1u},
        .stageFlags{VK_SHADER_STAGE_VERTEX_BIT},
        .pImmutableSamplers{nullptr}};

    const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .bindingCount{1u},
        .pBindings{&ubo_binding}};

    if (const VkResult result{vkCreateDescriptorSetLayout(
            context.device(), &descriptor_set_layout_create_info, nullptr,
            &resources.ubo_descriptor_set_layout)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  const std::array<VkDescriptorPoolSize, 1uz> descriptor_pool_sizes{
      VkDescriptorPoolSize{.type{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                           .descriptorCount{1u}}};

  const VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .maxSets{1u},
      .poolSizeCount{static_cast<std::uint32_t>(descriptor_pool_sizes.size())},
      .pPoolSizes{descriptor_pool_sizes.data()}};

  if (const VkResult result{
          vkCreateDescriptorPool(context.device(), &descriptor_pool_create_info,
                                 nullptr, &resources.descriptor_pool)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkDescriptorSetAllocateInfo ubo_descriptor_set_allocate_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
      .pNext{nullptr},
      .descriptorPool{resources.descriptor_pool},
      .descriptorSetCount{1u},
      .pSetLayouts{&resources.ubo_descriptor_set_layout}};

  if (const VkResult result{vkAllocateDescriptorSets(
          context.device(), &ubo_descriptor_set_allocate_info,
          &resources.ubo_descriptor_set)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkDescriptorBufferInfo buffer_info{
      .buffer{ubo_buffer.handle()},
      .offset{},
      .range{sizeof(uniform_buffer_object)}};

  const std::array<VkWriteDescriptorSet, 1uz> descriptor_set_writes{
      VkWriteDescriptorSet{.sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                           .pNext{nullptr},
                           .dstSet{resources.ubo_descriptor_set},
                           .dstBinding{},
                           .dstArrayElement{},
                           .descriptorCount{1u},
                           .descriptorType{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                           .pImageInfo{nullptr},
                           .pBufferInfo{&buffer_info},
                           .pTexelBufferView{nullptr}}};

  vkUpdateDescriptorSets(
      context.device(),
      static_cast<std::uint32_t>(descriptor_set_writes.size()),
      descriptor_set_writes.data(), 0, nullptr);

  const std::array<VkDescriptorSetLayout, 1uz> descriptor_set_layouts{
      resources.ubo_descriptor_set_layout};

  const VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .setLayoutCount{
          static_cast<std::uint32_t>(descriptor_set_layouts.size())},
      .pSetLayouts{descriptor_set_layouts.data()},
      .pushConstantRangeCount{},
      .pPushConstantRanges{nullptr}};

  if (const VkResult result{
          vkCreatePipelineLayout(context.device(), &pipeline_layout_info,
                                 nullptr, &resources.pipeline_layout)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const auto vertex_shader_module{create_shader_module(
      context.device(), "assets/shaders/vertexShader.spv")};
  if (!vertex_shader_module) {
    return std::unexpected{vertex_shader_module.error()};
  }

  const auto fragment_shader_module{create_shader_module(
      context.device(), "assets/shaders/fragmentShader.spv")};
  if (!fragment_shader_module) {
    vkDestroyShaderModule(context.device(), *vertex_shader_module, nullptr);
    return std::unexpected{fragment_shader_module.error()};
  }

  const std::array<VkPipelineShaderStageCreateInfo, 2uz> shader_stages{
      VkPipelineShaderStageCreateInfo{
          .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
          .pNext{nullptr},
          .flags{},
          .stage{VK_SHADER_STAGE_VERTEX_BIT},
          .module{*vertex_shader_module},
          .pName{"main"},
          .pSpecializationInfo{nullptr}},
      VkPipelineShaderStageCreateInfo{
          .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
          .pNext{nullptr},
          .flags{},
          .stage{VK_SHADER_STAGE_FRAGMENT_BIT},
          .module{*fragment_shader_module},
          .pName{"main"},
          .pSpecializationInfo{nullptr}}};

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
      .width{static_cast<float>(swapchain.extent.width)},
      .height{static_cast<float>(swapchain.extent.height)},
      .minDepth{0.0f},
      .maxDepth{1.0f}};

  const VkRect2D scissor{.offset{.x{0}, .y{0}}, .extent{swapchain.extent}};

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
      .layout{resources.pipeline_layout},
      .renderPass{swapchain.render_pass},
      .subpass{},
      .basePipelineHandle{VK_NULL_HANDLE},
      .basePipelineIndex{}};

  const VkResult result{
      vkCreateGraphicsPipelines(context.device(), VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr, &resources.pipeline)};

  vkDestroyShaderModule(context.device(), *fragment_shader_module, nullptr);
  vkDestroyShaderModule(context.device(), *vertex_shader_module, nullptr);

  if (result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return resources;
}

std::expected<realtime_mandelbrot_application::graphics_command_buffers,
              std::error_code>
realtime_mandelbrot_application::allocate_graphics_command_buffers(
    const vulkan_context &context, const std::uint32_t count) {
  const VkCommandBufferAllocateInfo command_buffer_allocate_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
      .pNext{nullptr},
      .commandPool{context.graphics_command_pool()},
      .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
      .commandBufferCount{count}};

  graphics_command_buffers command_buffers{};
  command_buffers.device = context.device();
  command_buffers.command_pool = context.graphics_command_pool();
  command_buffers.buffers.resize(count, VK_NULL_HANDLE);

  if (const VkResult result{vkAllocateCommandBuffers(
          context.device(), &command_buffer_allocate_info,
          command_buffers.buffers.data())};
      result != VK_SUCCESS) {
    command_buffers.buffers.clear();
    return make_vulkan_error(result);
  }

  return command_buffers;
}

std::expected<void, std::error_code>
realtime_mandelbrot_application::record_graphics_command_buffers(
    const swapchain_resources &swapchain, const pipeline_resources &pipeline,
    const vulkan_buffer &vertex_buffer, const vulkan_buffer &index_buffer,
    const graphics_command_buffers &command_buffers) {
  for (std::uint32_t i{0u}; i < swapchain.image_count; ++i) {
    const VkCommandBuffer command_buffer{command_buffers.buffers[i]};
    const VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
        .pNext{nullptr},
        .flags{},
        .pInheritanceInfo{nullptr}};

    const VkClearValue color_clear_value{{{0.0f, 0.0f, 0.0f, 1.0f}}};

    const VkRenderPassBeginInfo render_pass_begin_info{
        .sType{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO},
        .pNext{nullptr},
        .renderPass{swapchain.render_pass},
        .framebuffer{swapchain.framebuffers[i]},
        .renderArea{.offset{.x{0}, .y{0}}, .extent{swapchain.extent}},
        .clearValueCount{1u},
        .pClearValues{&color_clear_value}};

    const VkViewport viewport{
        .x{0.0f},
        .y{0.0f},
        .width{static_cast<float>(swapchain.extent.width)},
        .height{static_cast<float>(swapchain.extent.height)},
        .minDepth{0.0f},
        .maxDepth{1.0f}};

    const VkRect2D scissor{.offset{.x{0}, .y{0}}, .extent{swapchain.extent}};

    if (const VkResult result{
            vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    constexpr std::array<VkDeviceSize, 1uz> offsets{0};
    const VkBuffer vertex_buffer_handle{vertex_buffer.handle()};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_handle,
                           offsets.data());

    vkCmdBindIndexBuffer(command_buffer, index_buffer.handle(), 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline.pipeline);

    const std::array<VkDescriptorSet, 1uz> descriptor_sets{
        pipeline.ubo_descriptor_set};
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.pipeline_layout, 0,
                            static_cast<std::uint32_t>(descriptor_sets.size()),
                            descriptor_sets.data(), 0, nullptr);

    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    if (const VkResult result{vkEndCommandBuffer(command_buffer)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  return {};
}

std::expected<void, std::error_code>
realtime_mandelbrot_application::update_frame_data(const float delta_time,
                                                   const window &target_window,
                                                   uniform_buffer_object &ubo,
                                                   float &zoom_scale,
                                                   vulkan_buffer &ubo_buffer) {
  const auto [window_width, window_height]{target_window.get_size()};

  if (window_width == 0u || window_height == 0u) {
    return {};
  }

  const float aspect_ratio{static_cast<float>(window_width) /
                           static_cast<float>(window_height)};

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

    std::println("increased iteration count to: {}", ubo.iteration_count);
  }

  if (m_input.is_key_pressed(key_code::down)) {
    ubo.iteration_count -= 1;

    std::println("decreased iteration count to: {}", ubo.iteration_count);
  }

  /* Cap the zoom scale to avoid black border as we are rendering a quad */
  zoom_scale = zoom_scale > 1.0f * aspect_ratio ? 1.0f * aspect_ratio
                                                : std::fabs(zoom_scale);
  /* Update uniform buffer block */
  ubo.zoom_scale = zoom_scale;
  ubo.aspect_ratio = aspect_ratio;

  return ubo_buffer.write(std::as_bytes(std::span{&ubo, 1}));
}

std::expected<bool, std::error_code>
realtime_mandelbrot_application::draw_frame(
    const vulkan_context &context, swapchain_resources &swapchain,
    const graphics_command_buffers &command_buffers,
    std::uint32_t &frame_index) {
  if (const VkResult result{vkWaitForFences(
          context.device(), 1, &swapchain.in_flight_fences[frame_index],
          VK_TRUE, UINT64_MAX)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::uint32_t image_index{0u};
  const VkResult acquire_result{vkAcquireNextImageKHR(
      context.device(), swapchain.swapchain, max_swapchain_timeout,
      swapchain.present_complete[frame_index], VK_NULL_HANDLE, &image_index)};

  if (acquire_result != VK_SUCCESS) {
    return true;
  }

  if (swapchain.images_in_flight[image_index] != VK_NULL_HANDLE) {
    if (const VkResult result{vkWaitForFences(
            context.device(), 1, &swapchain.images_in_flight[image_index],
            VK_TRUE, UINT64_MAX)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  swapchain.images_in_flight[image_index] =
      swapchain.in_flight_fences[frame_index];

  const std::array<VkPipelineStageFlags, 1uz> wait_stages{
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  const VkSubmitInfo submit_info{
      .sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
      .pNext{nullptr},
      .waitSemaphoreCount{1u},
      .pWaitSemaphores{&swapchain.present_complete[frame_index]},
      .pWaitDstStageMask{wait_stages.data()},
      .commandBufferCount{1u},
      .pCommandBuffers{&command_buffers.buffers[image_index]},
      .signalSemaphoreCount{1u},
      .pSignalSemaphores{&swapchain.render_complete[image_index]}};

  if (const VkResult result{vkResetFences(
          context.device(), 1, &swapchain.in_flight_fences[frame_index])};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (const VkResult result{
          vkQueueSubmit(context.graphics_queue(), 1, &submit_info,
                        swapchain.in_flight_fences[frame_index])};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkPresentInfoKHR present_info{
      .sType{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR},
      .pNext{nullptr},
      .waitSemaphoreCount{1u},
      .pWaitSemaphores{&swapchain.render_complete[image_index]},
      .swapchainCount{1u},
      .pSwapchains{&swapchain.swapchain},
      .pImageIndices{&image_index},
      .pResults{nullptr}};

  const VkResult present_result{
      vkQueuePresentKHR(context.graphics_queue(), &present_info)};

  frame_index = (frame_index + 1u) % swapchain.max_frames_in_flight;

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
    return true;
  }

  if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
    return make_vulkan_error(present_result);
  }

  return false;
}

std::expected<void, std::error_code> realtime_mandelbrot_application::run() {
  auto created_context{vulkan_context::create(
      {.instance_extensions{window::get_required_extensions()}})};
  if (!created_context) {
    return std::unexpected{created_context.error()};
  }

  const vulkan_context context{std::move(*created_context)};

  auto created_window{window::create({.width{m_swapchain_extent.width},
                                      .height{m_swapchain_extent.height},
                                      .name{"Vulkan Mandelbrot Set Renderer"},
                                      .maximized{true}})};
  if (!created_window) {
    return std::unexpected{created_window.error()};
  }

  window app_window{std::move(*created_window)};

  auto created_surface{create_surface(context, app_window)};
  if (!created_surface) {
    return std::unexpected{created_surface.error()};
  }

  const surface_resource surface{std::move(*created_surface)};

  auto created_swapchain{create_swapchain_resources(context, surface.handle)};
  if (!created_swapchain) {
    return std::unexpected{created_swapchain.error()};
  }

  swapchain_resources swapchain{std::move(*created_swapchain)};

  constexpr std::array<float, 4uz * 3uz> fullscreen_quad_vertices{
      -1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
      1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};

  constexpr std::array<std::uint32_t, 6uz> fullscreen_quad_indices{0, 1, 2,
                                                                   2, 3, 0};

  auto created_vertex_buffer{create_device_local_buffer(
      context, std::as_bytes(std::span{fullscreen_quad_vertices}),
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)};
  if (!created_vertex_buffer) {
    return std::unexpected{created_vertex_buffer.error()};
  }

  const vulkan_buffer vertex_buffer{std::move(*created_vertex_buffer)};

  auto created_index_buffer{create_device_local_buffer(
      context, std::as_bytes(std::span{fullscreen_quad_indices}),
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT)};
  if (!created_index_buffer) {
    return std::unexpected{created_index_buffer.error()};
  }

  const vulkan_buffer index_buffer{std::move(*created_index_buffer)};

  auto created_ubo_buffer{vulkan_buffer::create(
      {.size{sizeof(uniform_buffer_object)},
       .device{context.device()},
       .physical_device{context.physical_device()},
       .usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
  if (!created_ubo_buffer) {
    return std::unexpected{created_ubo_buffer.error()};
  }

  vulkan_buffer ubo_buffer{std::move(*created_ubo_buffer)};

  auto created_pipeline{
      create_pipeline_resources(context, swapchain, ubo_buffer)};
  if (!created_pipeline) {
    return std::unexpected{created_pipeline.error()};
  }

  const pipeline_resources pipeline{std::move(*created_pipeline)};

  auto created_command_buffers{
      allocate_graphics_command_buffers(context, swapchain.image_count)};
  if (!created_command_buffers) {
    return std::unexpected{created_command_buffers.error()};
  }

  graphics_command_buffers command_buffers{std::move(*created_command_buffers)};

  if (const auto recorded{record_graphics_command_buffers(
          swapchain, pipeline, vertex_buffer, index_buffer, command_buffers)};
      !recorded) {
    return recorded;
  }

  const auto [window_width, window_height]{app_window.get_size()};
  float zoom_scale{1.0f};
  uniform_buffer_object ubo{
      .aspect_ratio{window_height == 0u
                        ? 1.0f
                        : static_cast<float>(window_width) /
                              static_cast<float>(window_height)},
      .center_x{0.5f},
      .center_y{0.0f},
      .zoom_scale{zoom_scale},
      .iteration_count{800},
      .padding_x{},
      .padding_y{},
      .padding_z{}};

  if (const auto written{ubo_buffer.write(std::as_bytes(std::span{&ubo, 1}))};
      !written) {
    return written;
  }

  std::uint32_t frame_index{0u};
  auto previous_time{std::chrono::steady_clock::now()};

  while (m_running) {
    app_window.poll([this](const event &polled) { on_event(polled); });
    const auto current_time{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> delta_time{current_time - previous_time};
    previous_time = current_time;

    if (const auto updated{update_frame_data(delta_time.count(), app_window,
                                             ubo, zoom_scale, ubo_buffer)};
        !updated) {
      return updated;
    }

    const auto drawn{
        draw_frame(context, swapchain, command_buffers, frame_index)};
    if (!drawn) {
      return std::unexpected{drawn.error()};
    }

    if (!*drawn) {
      continue;
    }

    const auto [width, height]{app_window.get_size()};
    m_swapchain_extent = VkExtent2D{.width{width}, .height{height}};

    while (
        (m_swapchain_extent.width == 0u || m_swapchain_extent.height == 0u) &&
        m_running) {
      app_window.poll([this](const event &polled) { on_event(polled); });
      const auto [polled_width, polled_height]{app_window.get_size()};
      m_swapchain_extent =
          VkExtent2D{.width{polled_width}, .height{polled_height}};
    }

    if (!m_running) {
      break;
    }

    if (const VkResult result{vkDeviceWaitIdle(context.device())};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    swapchain = swapchain_resources{};

    auto recreated_swapchain{
        create_swapchain_resources(context, surface.handle)};
    if (!recreated_swapchain) {
      return std::unexpected{recreated_swapchain.error()};
    }

    swapchain = std::move(*recreated_swapchain);

    auto reallocated_command_buffers{
        allocate_graphics_command_buffers(context, swapchain.image_count)};
    if (!reallocated_command_buffers) {
      return std::unexpected{reallocated_command_buffers.error()};
    }

    command_buffers = std::move(*reallocated_command_buffers);

    if (const auto recorded{record_graphics_command_buffers(
            swapchain, pipeline, vertex_buffer, index_buffer, command_buffers)};
        !recorded) {
      return recorded;
    }

    frame_index = 0u;
  }

  if (const VkResult result{vkDeviceWaitIdle(context.device())};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}
