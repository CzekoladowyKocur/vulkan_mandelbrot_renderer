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

namespace Utilities {
constexpr uint64_t MaxSwapchainTimeout = UINT64_MAX;
} // namespace Utilities

bool realtime_mandelbrot_application::Initialize() {
  auto context{vulkan_context::create(
      {.instance_extensions{window::get_required_extensions()}})};
  if (!context) {
    std::println("Failed to create vulkan context: {}",
                 context.error().message());
    return false;
  }

  m_Context = std::move(*context);

  if (!LoadAssets()) {
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

  if (!CreateSurface()) {
    std::println("Failed to create vulkan surface");
    return false;
  }

  if (!CreateSwapchain()) {
    std::println("Failed to create vulkan swapchain");
    return false;
  }

  if (!CreateGraphicsBasedPipeline()) {
    std::println("Failed to create graphics based pipeline");
    return false;
  }

  if (!AllocateGraphicsCommandBuffers()) {
    std::println("Failed to allocate graphics command buffers");
    return false;
  }

  if (!RecordGraphicsCommandBuffers()) {
    std::println("Failed to create graphics command buffers");
    return false;
  }

  return true;
}

bool realtime_mandelbrot_application::Run() {
  if (!m_window.has_value()) {
    return false;
  }

  window &active_window{*m_window};

  auto previous_time{std::chrono::steady_clock::now()};

  while (m_Running) {
    /* Poll events */

    active_window.poll([this](const event &polled) { OnEvent(polled); });
    const auto current_time{std::chrono::steady_clock::now()};
    const std::chrono::duration<float> delta_time{current_time - previous_time};
    previous_time = current_time;

    UpdateFrameData(delta_time.count());
    DrawFrame();
  }

  return true;
}

bool realtime_mandelbrot_application::Shutdown() {
  VK_CHECK(vkDeviceWaitIdle(m_Context.device()));
  m_ColorPaletteTexture.reset();
  /* Device level */
  VK_CHECK(vkDeviceWaitIdle(m_Context.device()));

  /* Graphics */
  /* Destroy buffers */
  m_UBOBuffer.reset();
  m_VertexBuffer.reset();
  m_IndexBuffer.reset();

  /* Destroy Pipelines */
  if (m_GraphicsPipeline)
    vkDestroyPipeline(m_Context.device(), m_GraphicsPipeline, nullptr);

  if (m_GraphicsPipelineLayout)
    vkDestroyPipelineLayout(m_Context.device(), m_GraphicsPipelineLayout,
                            nullptr);

  if (m_GraphicsPipelineUBOBufferDescriptorSetLayout)
    vkDestroyDescriptorSetLayout(m_Context.device(),
                                 m_GraphicsPipelineUBOBufferDescriptorSetLayout,
                                 nullptr);

  if (m_GraphicsPipelineColorPaletteDescriptorSetLayout)
    vkDestroyDescriptorSetLayout(
        m_Context.device(), m_GraphicsPipelineColorPaletteDescriptorSetLayout,
        nullptr);

  if (m_GraphicsPipelineColorPaletteDescriptorSet)
    vkFreeDescriptorSets(m_Context.device(), m_GraphicsPipelineDescriptorPool,
                         1, &m_GraphicsPipelineColorPaletteDescriptorSet);

  if (m_GraphicsPipelineDescriptorPool)
    vkDestroyDescriptorPool(m_Context.device(),
                            m_GraphicsPipelineDescriptorPool, nullptr);

  CleanupSwapchain();

  /* Instance level */
  vkDestroySurfaceKHR(m_Context.instance(), m_Surface, nullptr);

  m_Context = {};

  return true;
}

void realtime_mandelbrot_application::OnEvent(const event &polled_event) {
  std::visit(overloaded{
                 [this](const window_close_event &) { m_Running = false; },

                 [this](const window_resize_event &resize) {
                   m_SwapchainExtent.width = resize.width;
                   m_SwapchainExtent.height = resize.height;

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

bool realtime_mandelbrot_application::CreateSurface() {
  if (!m_window.has_value()) {
    return false;
  }

  const auto surface{m_window->get_surface(m_Context.instance())};

  if (!surface) {
    std::println("Failed to create vulkan surface: {}",
                 surface.error().message());
    return false;
  }

  m_Surface = *surface;

  VkBool32 supported;
  vkGetPhysicalDeviceSurfaceSupportKHR(m_Context.physical_device(),
                                       m_Context.graphics_queue_family(),
                                       m_Surface, &supported);
  return true;
}

using b8 = bool;
using u32 = uint32_t;

bool realtime_mandelbrot_application::CreateSwapchain() {
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      m_Context.physical_device(), m_Surface, &m_SurfaceCapabilities));

  uint32_t surfaceFormatCount;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_Context.physical_device(), m_Surface, &surfaceFormatCount, nullptr));
  assert(surfaceFormatCount > 0);

  std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(surfaceFormatCount);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_Context.physical_device(), m_Surface, &surfaceFormatCount,
      availableSurfaceFormats.data()));

  m_SurfaceFormat = availableSurfaceFormats[0];
  for (const VkSurfaceFormatKHR surfaceFormat : availableSurfaceFormats)
    if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
        surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      m_SurfaceFormat = surfaceFormat;
      break;
    }

  uint32_t presentModeCount;
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_Context.physical_device(), m_Surface, &presentModeCount, nullptr));
  assert(presentModeCount > 0);

  std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_Context.physical_device(), m_Surface, &presentModeCount,
      availablePresentModes.data()));

  /* The only present mode guaranteed to be supported by the specification */
  m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (const VkPresentModeKHR presentMode : availablePresentModes)
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      m_PresentMode = presentMode;
      break;
    }

  m_MaxFramesInFlight =
      m_PresentMode == VK_PRESENT_MODE_MAILBOX_KHR ? (3) : (2);

  const VkExtent2D minSwapchainImageExtent =
      m_SurfaceCapabilities.minImageExtent;
  const VkExtent2D maxSwapchainImageExtent =
      m_SurfaceCapabilities.maxImageExtent;

  /* Clamp */
  m_SwapchainExtent.width =
      std::clamp(m_SwapchainExtent.width, minSwapchainImageExtent.width,
                 maxSwapchainImageExtent.width);
  m_SwapchainExtent.height =
      std::clamp(m_SwapchainExtent.height, minSwapchainImageExtent.height,
                 maxSwapchainImageExtent.height);

  const uint32_t minImageCount = m_SurfaceCapabilities.minImageCount;
  const uint32_t maxImageCount = m_SurfaceCapabilities.maxImageCount;
  m_ImageCount =
      (minImageCount + 1) < maxImageCount ? (minImageCount + 1) : maxImageCount;

  VkSwapchainCreateInfoKHR swapchainCreateInfo;
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface = m_Surface;
  swapchainCreateInfo.imageFormat = m_SurfaceFormat.format;
  swapchainCreateInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
  swapchainCreateInfo.presentMode = m_PresentMode;
  swapchainCreateInfo.imageExtent = m_SwapchainExtent;
  swapchainCreateInfo.minImageCount = m_ImageCount;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCreateInfo.queueFamilyIndexCount =
      0; // queuesShared ? 0 : 2; /* One for graphics and one for present if not
         // shared */
  swapchainCreateInfo.pQueueFamilyIndices =
      nullptr; // queuesShared ? nullptr : queueFamilyIndices;
  swapchainCreateInfo.imageSharingMode =
      VK_SHARING_MODE_EXCLUSIVE; // queuesShared ? VK_SHARING_MODE_EXCLUSIVE :
                                 // VK_SHARING_MODE_CONCURRENT;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.preTransform = m_SurfaceCapabilities.currentTransform;
  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
  swapchainCreateInfo.flags = 0;
  swapchainCreateInfo.pNext = nullptr;

  if (vkCreateSwapchainKHR(m_Context.device(), &swapchainCreateInfo, nullptr,
                           &m_Swapchain) != VK_SUCCESS) {
    std::println("Failed to create swapchain");
    return false;
  }

  m_ImageCount = 0;
  m_SwapchainImages.clear();
  VK_CHECK(vkGetSwapchainImagesKHR(m_Context.device(), m_Swapchain,
                                   &m_ImageCount, nullptr));

  m_SwapchainImages.resize(m_ImageCount);
  m_SwapchainImageViews.resize(m_ImageCount);
  m_SwapchainFramebuffers.resize(m_ImageCount);

  VK_CHECK(vkGetSwapchainImagesKHR(m_Context.device(), m_Swapchain,
                                   &m_ImageCount, m_SwapchainImages.data()));

  VkAttachmentDescription colorAttachment;
  colorAttachment.format = m_SurfaceFormat.format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  colorAttachment.flags = 0;

  VkAttachmentReference colorAttachmentReference;
  colorAttachmentReference.attachment = 0;
  colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription;
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorAttachmentReference;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = nullptr;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = nullptr;
  subpassDescription.pResolveAttachments = nullptr;
  subpassDescription.pDepthStencilAttachment = nullptr;
  subpassDescription.flags = 0;

  VkSubpassDependency subpassDependency;
  subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependency.dstSubpass = 0; /* What subpass we are writing to */
  subpassDependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependency.srcAccessMask = 0;
  subpassDependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpassDependency.dependencyFlags = 0;

  const std::array<VkAttachmentDescription, 1> attachments = {colorAttachment};
  VkRenderPassCreateInfo renderPassCreateInfo;
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount =
      static_cast<uint32_t>(attachments.size());
  renderPassCreateInfo.pAttachments = attachments.data();
  renderPassCreateInfo.dependencyCount = 1;
  renderPassCreateInfo.pDependencies = &subpassDependency;
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDescription;
  renderPassCreateInfo.flags = 0;
  renderPassCreateInfo.pNext = nullptr;

  VK_CHECK(vkCreateRenderPass(m_Context.device(), &renderPassCreateInfo,
                              nullptr, &m_SwapchainRenderPass));

  uint32_t imageIndex = 0;
  for (const VkImage image : m_SwapchainImages) {
    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = m_SurfaceFormat.format;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.pNext = nullptr;

    VK_CHECK(vkCreateImageView(m_Context.device(), &imageViewCreateInfo,
                               nullptr, &m_SwapchainImageViews[imageIndex]));

    VkFramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = m_SwapchainRenderPass;
    framebufferCreateInfo.width = m_SwapchainExtent.width;
    framebufferCreateInfo.height = m_SwapchainExtent.height;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = &m_SwapchainImageViews[imageIndex];
    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.flags = 0;
    framebufferCreateInfo.pNext = nullptr;

    VK_CHECK(vkCreateFramebuffer(m_Context.device(), &framebufferCreateInfo,
                                 nullptr,
                                 &m_SwapchainFramebuffers[imageIndex]));

    ++imageIndex;
  }

  VkSemaphoreCreateInfo semaphoreCreateInfo;
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreCreateInfo.flags = VK_SEMAPHORE_TYPE_BINARY;
  semaphoreCreateInfo.pNext = nullptr;

  VkFenceCreateInfo fenceCreateInfo;
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  fenceCreateInfo.pNext = nullptr;

  m_Semaphores.PresentComplete.resize(m_MaxFramesInFlight);
  m_Semaphores.RenderComplete.resize(m_MaxFramesInFlight);
  m_InFlightFences.resize(m_MaxFramesInFlight);
  for (uint32_t i = 0; i < m_MaxFramesInFlight; ++i) {
    VK_CHECK(vkCreateSemaphore(m_Context.device(), &semaphoreCreateInfo,
                               nullptr, &m_Semaphores.PresentComplete[i]));

    VK_CHECK(vkCreateSemaphore(m_Context.device(), &semaphoreCreateInfo,
                               nullptr, &m_Semaphores.RenderComplete[i]));

    VK_CHECK(vkCreateFence(m_Context.device(), &fenceCreateInfo, nullptr,
                           &m_InFlightFences[i]));
  }

  m_ImagesInFlight.resize(m_ImageCount, VK_NULL_HANDLE);
  return true;
}

bool realtime_mandelbrot_application::LoadAssets() {
  const auto palette_image{image_2d::create("assets/images/violetPalette.bmp")};
  if (!palette_image) {
    return false;
  }

  auto palette{
      texture_2d::create({.image{*palette_image},
                          .device{m_Context.device()},
                          .physical_device{m_Context.physical_device()},
                          .command_pool{m_Context.graphics_command_pool()},
                          .queue{m_Context.graphics_queue()}})};
  if (!palette) {
    return false;
  }

  m_ColorPaletteTexture.emplace(std::move(*palette));

  return true;
}

bool realtime_mandelbrot_application::CreateGraphicsBasedPipeline() {
  constexpr VkDeviceSize vertexBufferSize = sizeof(float) * 4 * 3;
  constexpr VkDeviceSize indexBufferSize = sizeof(uint32_t) * 6;

  const std::array<float, 4ull * 3ull> fullscreenQuadVertices

      {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
       1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};

  const std::array<uint32_t, 6ull> fullscreenQuadIndices{0, 1, 2, 2, 3, 0};

  const single_time_command_context uploadContext{
      .device{m_Context.device()},
      .command_pool{m_Context.graphics_command_pool()},
      .queue{m_Context.graphics_queue()}};

  {
    auto staging{vulkan_buffer::create(
        {.size{vertexBufferSize},
         .device{m_Context.device()},
         .physical_device{m_Context.physical_device()},
         .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};

    if (!staging) {
      return false;
    }

    if (!staging->write(std::as_bytes(std::span{fullscreenQuadVertices}))) {
      return false;
    }

    auto vertexBuffer{vulkan_buffer::create(
        {.size{vertexBufferSize},
         .device{m_Context.device()},
         .physical_device{m_Context.physical_device()},
         .usage{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}})};

    if (!vertexBuffer) {
      return false;
    }

    if (!copy_buffer(uploadContext, staging->handle(), vertexBuffer->handle(),
                     vertexBufferSize)) {
      return false;
    }

    m_VertexBuffer.emplace(std::move(*vertexBuffer));
  }

  {
    auto staging{vulkan_buffer::create(
        {.size{indexBufferSize},
         .device{m_Context.device()},
         .physical_device{m_Context.physical_device()},
         .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
    if (!staging) {
      return false;
    }

    if (!staging->write(std::as_bytes(std::span{fullscreenQuadIndices}))) {
      return false;
    }

    auto indexBuffer{vulkan_buffer::create(
        {.size{indexBufferSize},
         .device{m_Context.device()},
         .physical_device{m_Context.physical_device()},
         .usage{VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT},
         .memory_flags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT}})};
    if (!indexBuffer) {
      return false;
    }

    if (!copy_buffer(uploadContext, staging->handle(), indexBuffer->handle(),
                     indexBufferSize)) {
      return false;
    }

    m_IndexBuffer.emplace(std::move(*indexBuffer));
  }

  /* TODO: Add support for doubles */
  const bool deviceSupportsDoublePrecisionFloats =
      false; // m_Context.physical_device()Features.shaderFloat64;
  const auto vertexShaderModule{create_shader_module(
      m_Context.device(), deviceSupportsDoublePrecisionFloats
                              ? "assets/shaders/vertexShaderDoublePrecision.spv"
                              : "assets/shaders/vertexShader.spv")};
  if (!vertexShaderModule) {
    std::println("Failed to create vertex shader module");
    return false;
  }
  m_VertexShaderModule = *vertexShaderModule;

  const auto fragmentShaderModule{create_shader_module(
      m_Context.device(),
      deviceSupportsDoublePrecisionFloats
          ? "assets/shaders/fragmentShaderDoublePrecision.spv"
          : "assets/shaders/fragmentShader.spv")};
  if (!fragmentShaderModule) {
    std::println("Failed to create fragment shader module");
    return false;
  }
  m_FragmentShaderModule = *fragmentShaderModule;

  VkPipelineShaderStageCreateInfo vertShaderStageInfo;
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = m_VertexShaderModule;
  vertShaderStageInfo.pName = "main";
  vertShaderStageInfo.pSpecializationInfo = nullptr;
  vertShaderStageInfo.flags = 0;
  vertShaderStageInfo.pNext = nullptr;

  VkPipelineShaderStageCreateInfo fragShaderStageInfo;
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = m_FragmentShaderModule;
  fragShaderStageInfo.pName = "main";
  fragShaderStageInfo.pSpecializationInfo = nullptr;
  fragShaderStageInfo.flags = 0;
  fragShaderStageInfo.pNext = nullptr;

  const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
      vertShaderStageInfo, fragShaderStageInfo};
  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding = 0;
  vertexInputBindingDescription.stride = sizeof(float) * 3;
  vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttributeDescription;
  vertexInputAttributeDescription.binding = 0;
  vertexInputAttributeDescription.location = 0;
  vertexInputAttributeDescription.offset = 0;
  vertexInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = 1;
  vertexInputInfo.pVertexAttributeDescriptions =
      &vertexInputAttributeDescription;
  vertexInputInfo.flags = 0;
  vertexInputInfo.pNext = nullptr;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly;
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  inputAssembly.flags = 0;
  inputAssembly.pNext = nullptr;

  VkViewport viewport;
  viewport.width = static_cast<float>(m_SwapchainExtent.width);
  viewport.height = static_cast<float>(m_SwapchainExtent.height);
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset = {.x{0}, .y{0}};
  scissor.extent = m_SwapchainExtent;
  VkPipelineViewportStateCreateInfo viewportState;
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;
  viewportState.flags = 0;
  viewportState.pNext = nullptr;

  const std::array<VkDynamicState, 2> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicStateInfo;
  dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(dynamicStates.size());
  dynamicStateInfo.pDynamicStates = dynamicStates.data();
  dynamicStateInfo.flags = 0;
  dynamicStateInfo.pNext = nullptr;

  VkPipelineRasterizationStateCreateInfo rasterizerStateInfo{};
  rasterizerStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizerStateInfo.depthClampEnable = VK_FALSE;
  rasterizerStateInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizerStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizerStateInfo.lineWidth = 1.0f;
  rasterizerStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizerStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizerStateInfo.depthBiasEnable = VK_FALSE;
  rasterizerStateInfo.flags = 0;
  rasterizerStateInfo.pNext = nullptr;

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO},
      .rasterizationSamples{VK_SAMPLE_COUNT_1_BIT}};
  multisampling.sampleShadingEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  {
    VkDescriptorSetLayoutBinding uboBinding;
    uboBinding.binding = 0;
    uboBinding.descriptorCount = 1;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{uboBinding};
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount =
        static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCreateInfo.pBindings = bindings.data();
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.pNext = nullptr;

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_Context.device(), &descriptorSetLayoutCreateInfo, nullptr,
        &m_GraphicsPipelineUBOBufferDescriptorSetLayout));
  }

  {
    VkDescriptorSetLayoutBinding colorPalleteBinding;
    colorPalleteBinding.binding = 0;
    colorPalleteBinding.descriptorCount = 1;
    colorPalleteBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    colorPalleteBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    colorPalleteBinding.pImmutableSamplers = nullptr;

    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{
        colorPalleteBinding};
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount =
        static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCreateInfo.pBindings = bindings.data();
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.pNext = nullptr;

    VK_CHECK(vkCreateDescriptorSetLayout(
        m_Context.device(), &descriptorSetLayoutCreateInfo, nullptr,
        &m_GraphicsPipelineColorPaletteDescriptorSetLayout));
  }

  const std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts{
      m_GraphicsPipelineUBOBufferDescriptorSetLayout,
      m_GraphicsPipelineColorPaletteDescriptorSetLayout};
  VkPipelineLayoutCreateInfo pipelineLayoutInfo;
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
      static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;
  pipelineLayoutInfo.flags = 0;
  pipelineLayoutInfo.pNext = nullptr;

  if (!m_ColorPaletteTexture.has_value()) {
    return false;
  }

  VkDescriptorImageInfo imageInfo;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = m_ColorPaletteTexture->GetImageView();
  imageInfo.sampler = m_ColorPaletteTexture->GetImageSampler();

  VkDescriptorPoolSize uboBufferdescriptorPoolSize;
  uboBufferdescriptorPoolSize.descriptorCount = 1;
  uboBufferdescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  VkDescriptorPoolSize colorPalleteImagedescriptorPoolSize;
  colorPalleteImagedescriptorPoolSize.descriptorCount = 1;
  colorPalleteImagedescriptorPoolSize.type =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

  const std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes{
      uboBufferdescriptorPoolSize, colorPalleteImagedescriptorPoolSize};
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
  descriptorPoolCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.poolSizeCount =
      static_cast<uint32_t>(descriptorPoolSizes.size());
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
  descriptorPoolCreateInfo.maxSets = 10;
  descriptorPoolCreateInfo.flags =
      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolCreateInfo.pNext = nullptr;

  VK_CHECK(vkCreateDescriptorPool(m_Context.device(), &descriptorPoolCreateInfo,
                                  nullptr, &m_GraphicsPipelineDescriptorPool));

  VkDescriptorSetAllocateInfo uboBufferDescriptorSetAllocateInfo;
  uboBufferDescriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  uboBufferDescriptorSetAllocateInfo.descriptorPool =
      m_GraphicsPipelineDescriptorPool;
  uboBufferDescriptorSetAllocateInfo.descriptorSetCount = 1;
  uboBufferDescriptorSetAllocateInfo.pSetLayouts =
      &m_GraphicsPipelineUBOBufferDescriptorSetLayout;
  uboBufferDescriptorSetAllocateInfo.pNext = nullptr;

  VK_CHECK(vkAllocateDescriptorSets(m_Context.device(),
                                    &uboBufferDescriptorSetAllocateInfo,
                                    &m_GraphicsPipelineUBOBufferDescriptorSet));

  VkDescriptorSetAllocateInfo colorPalleteImageDescriptorSetAllocateInfo;
  colorPalleteImageDescriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  colorPalleteImageDescriptorSetAllocateInfo.descriptorPool =
      m_GraphicsPipelineDescriptorPool;
  colorPalleteImageDescriptorSetAllocateInfo.descriptorSetCount = 1;
  colorPalleteImageDescriptorSetAllocateInfo.pSetLayouts =
      &m_GraphicsPipelineColorPaletteDescriptorSetLayout;
  colorPalleteImageDescriptorSetAllocateInfo.pNext = nullptr;

  VK_CHECK(vkAllocateDescriptorSets(
      m_Context.device(), &colorPalleteImageDescriptorSetAllocateInfo,
      &m_GraphicsPipelineColorPaletteDescriptorSet));

  auto uboBuffer{vulkan_buffer::create(
      {.size{sizeof(UBO)},
       .device{m_Context.device()},
       .physical_device{m_Context.physical_device()},
       .usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}})};
  if (!uboBuffer) {
    return false;
  }

  m_UBOBuffer.emplace(std::move(*uboBuffer));

  VkWriteDescriptorSet colorPalleteDescriptorSetWrite{};
  colorPalleteDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  colorPalleteDescriptorSetWrite.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  colorPalleteDescriptorSetWrite.dstBinding = 0;
  colorPalleteDescriptorSetWrite.dstArrayElement = 0;
  colorPalleteDescriptorSetWrite.descriptorCount = 1;
  colorPalleteDescriptorSetWrite.dstSet =
      m_GraphicsPipelineColorPaletteDescriptorSet;
  colorPalleteDescriptorSetWrite.pBufferInfo = nullptr;
  colorPalleteDescriptorSetWrite.pImageInfo = &imageInfo;
  colorPalleteDescriptorSetWrite.pTexelBufferView = nullptr;
  colorPalleteDescriptorSetWrite.pNext = nullptr;

  vkUpdateDescriptorSets(m_Context.device(), 1, &colorPalleteDescriptorSetWrite,
                         0, nullptr);

  const auto temporary = glm::mat4(1.0f);
  if (!m_UBOBuffer->write(
          std::as_bytes(std::span{&temporary, 1}).first(sizeof(UBO)))) {
    return false;
  }

  VkDescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = m_UBOBuffer->handle();
  bufferInfo.range = sizeof(UBO);
  bufferInfo.offset = 0;

  VkWriteDescriptorSet uboBufferDescriptorSetWrite{};
  uboBufferDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  uboBufferDescriptorSetWrite.descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboBufferDescriptorSetWrite.dstBinding = 0;
  uboBufferDescriptorSetWrite.dstArrayElement = 0;
  uboBufferDescriptorSetWrite.descriptorCount = 1;
  uboBufferDescriptorSetWrite.dstSet = m_GraphicsPipelineUBOBufferDescriptorSet;
  uboBufferDescriptorSetWrite.pBufferInfo = &bufferInfo;
  uboBufferDescriptorSetWrite.pImageInfo = nullptr;
  uboBufferDescriptorSetWrite.pTexelBufferView = nullptr;
  uboBufferDescriptorSetWrite.pNext = nullptr;

  vkUpdateDescriptorSets(m_Context.device(), 1, &uboBufferDescriptorSetWrite, 0,
                         nullptr);

  if (vkCreatePipelineLayout(m_Context.device(), &pipelineLayoutInfo, nullptr,
                             &m_GraphicsPipelineLayout) != VK_SUCCESS) {
    std::println("Failed to create graphics pipeline layout");
    return false;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizerStateInfo;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = m_GraphicsPipelineLayout;
  pipelineInfo.renderPass = m_SwapchainRenderPass;
  pipelineInfo.pDynamicState = &dynamicStateInfo;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_Context.device(), VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &m_GraphicsPipeline) != VK_SUCCESS) {
    std::println("Failed to create graphics pipeline");
    return false;
  }

  vkDestroyShaderModule(m_Context.device(), m_FragmentShaderModule, nullptr);

  vkDestroyShaderModule(m_Context.device(), m_VertexShaderModule, nullptr);

  return true;
}

bool realtime_mandelbrot_application::AllocateGraphicsCommandBuffers() {
  VkCommandBufferAllocateInfo commandBufferAllocateInfo;
  commandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = m_Context.graphics_command_pool();
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = m_ImageCount;
  commandBufferAllocateInfo.pNext = nullptr;

  m_GraphicsPipelineCommandBuffers.resize(m_ImageCount);
  VK_CHECK(vkAllocateCommandBuffers(m_Context.device(),
                                    &commandBufferAllocateInfo,
                                    m_GraphicsPipelineCommandBuffers.data()));

  return true;
}

bool realtime_mandelbrot_application::RecordGraphicsCommandBuffers() {
  if (!m_VertexBuffer.has_value() || !m_IndexBuffer.has_value()) {
    return false;
  }

  for (uint32_t i = 0; i < m_ImageCount; ++i) {
    VkCommandBuffer &commandBuffer = m_GraphicsPipelineCommandBuffers[i];
    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pNext = nullptr;

    VkClearValue colorClearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    std::array<VkClearValue, 1u> clearValues{colorClearValue};

    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.framebuffer = m_SwapchainFramebuffers[i];
    renderPassBeginInfo.renderPass = m_SwapchainRenderPass;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues.data();
    renderPassBeginInfo.renderArea.extent = m_SwapchainExtent;
    renderPassBeginInfo.renderArea.offset = {.x{0}, .y{0}};
    renderPassBeginInfo.pNext = nullptr;

    VkViewport viewport;
    viewport.width = static_cast<float>(m_SwapchainExtent.width);
    viewport.height = static_cast<float>(m_SwapchainExtent.height);
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.extent = m_SwapchainExtent;
    scissor.offset = {.x{0}, .y{0}};

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    constexpr std::array<VkDeviceSize, 1ull> offsets{0};
    const VkBuffer vertexBufferHandle{m_VertexBuffer->handle()};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBufferHandle,
                           offsets.data());

    vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->handle(), 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_GraphicsPipeline);

    const std::array<VkDescriptorSet, 2> descriptorSets{
        m_GraphicsPipelineUBOBufferDescriptorSet,
        m_GraphicsPipelineColorPaletteDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_GraphicsPipelineLayout, 0,
                            static_cast<uint32_t>(descriptorSets.size()),
                            descriptorSets.data(), 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
  }

  return true;
}

void realtime_mandelbrot_application::UpdateFrameData(const float deltaTime) {
  static float zoomScale = 1.0f;
  if (!m_window.has_value()) {
    return;
  }

  const auto [windowWidth, windowHeight] = m_window->get_size();

  if (windowWidth <= 0 || windowHeight <= 0)
    return;

  const float aspectRatio =
      static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
  static UBO ubo = {
      .AspectRatio{aspectRatio},
      .CenterX{0.5f},
      .CenterY{0.0f},
      .ZoomScale{zoomScale},
      .IterationCount{800},
      .padding_x{},
      .padding_y{},
      .padding_z{},
  };

  constexpr float moveSpeedFactor = 0.25f;
  constexpr float zoomSpeedFactor = 1.0f;

  const float zoomSpeed = zoomSpeedFactor;
  const float moveSpeed = m_input.is_key_pressed(key_code::shift)
                              ? moveSpeedFactor * 2.0f
                              : moveSpeedFactor;
  /* Move */
  if (m_input.is_key_pressed(key_code::z))
    zoomScale += zoomScale * zoomSpeed * deltaTime;

  if (m_input.is_key_pressed(key_code::x))
    zoomScale -= zoomScale * zoomSpeed * deltaTime;

  if (m_input.is_key_pressed(key_code::w))
    ubo.CenterY += moveSpeed * deltaTime * zoomScale;

  if (m_input.is_key_pressed(key_code::s))
    ubo.CenterY -= moveSpeed * deltaTime * zoomScale;

  if (m_input.is_key_pressed(key_code::a))
    ubo.CenterX += moveSpeed * deltaTime * zoomScale;

  if (m_input.is_key_pressed(key_code::d))
    ubo.CenterX -= moveSpeed * deltaTime * zoomScale;

  if (m_input.is_key_pressed(key_code::up))
    ubo.IterationCount += 1;

  if (m_input.is_key_pressed(key_code::down))
    ubo.IterationCount -= 1;

  /* Cap the zoom scale to avoid black border as we are rendering a quad */
  zoomScale =
      zoomScale > 1.0f * aspectRatio ? 1.0f * aspectRatio : fabs(zoomScale);
  /* Update uniform buffer block */
  ubo.ZoomScale = zoomScale;
  ubo.AspectRatio = aspectRatio;

  if (!m_UBOBuffer.has_value() ||
      !m_UBOBuffer->write(std::as_bytes(std::span{&ubo, 1}))) {
    return;
  }

  VkDescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = m_UBOBuffer->handle();
  bufferInfo.range = sizeof(UBO);
  bufferInfo.offset = 0;

  VkWriteDescriptorSet uboBufferDescriptorSetWrite{};
  uboBufferDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  uboBufferDescriptorSetWrite.descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboBufferDescriptorSetWrite.dstBinding = 0;
  uboBufferDescriptorSetWrite.dstArrayElement = 0;
  uboBufferDescriptorSetWrite.descriptorCount = 1;
  uboBufferDescriptorSetWrite.dstSet = m_GraphicsPipelineUBOBufferDescriptorSet;
  uboBufferDescriptorSetWrite.pBufferInfo = &bufferInfo;
  uboBufferDescriptorSetWrite.pImageInfo = nullptr;
  uboBufferDescriptorSetWrite.pTexelBufferView = nullptr;
  uboBufferDescriptorSetWrite.pNext = nullptr;
}

void realtime_mandelbrot_application::DrawFrame() {
  VkResult result = vkAcquireNextImageKHR(
      m_Context.device(), m_Swapchain, Utilities::MaxSwapchainTimeout,
      m_Semaphores.PresentComplete[m_FrameIndex], VK_NULL_HANDLE,
      &m_ImageIndex);

  if (result != VK_SUCCESS) {
    if (m_window.has_value()) {
      const auto [windowWidth, windowHeight] = m_window->get_size();
      RecreateSwapchain(windowWidth, windowHeight);
    }
    return;
  }

  if (m_ImagesInFlight[m_ImageIndex] != VK_NULL_HANDLE)
    vkWaitForFences(m_Context.device(), 1, &m_ImagesInFlight[m_ImageIndex],
                    VK_TRUE, UINT64_MAX);

  m_ImagesInFlight[m_ImageIndex] = m_InFlightFences[m_FrameIndex];

  const std::array<VkPipelineStageFlags, 1ull> waitStages{
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_GraphicsPipelineCommandBuffers[m_ImageIndex];
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &m_Semaphores.PresentComplete[m_FrameIndex];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &m_Semaphores.RenderComplete[m_FrameIndex];
  submitInfo.pWaitDstStageMask = waitStages.data();

  VK_CHECK(
      vkResetFences(m_Context.device(), 1, &m_InFlightFences[m_FrameIndex]));

  VK_CHECK(vkQueueSubmit(m_Context.graphics_queue(), 1, &submitInfo,
                         m_InFlightFences[m_FrameIndex]));

  VkPresentInfoKHR presentInfo;
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_Swapchain;
  presentInfo.pImageIndices = &m_ImageIndex;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &m_Semaphores.RenderComplete[m_FrameIndex];
  presentInfo.pResults = nullptr;
  presentInfo.pNext = nullptr;

  result = vkQueuePresentKHR(m_Context.graphics_queue(), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR && m_window.has_value()) {
    const auto [windowWidth, windowHeight] = m_window->get_size();
    RecreateSwapchain(windowWidth, windowHeight);
  }

  m_FrameIndex = (m_FrameIndex + 1) % m_MaxFramesInFlight;
}

void realtime_mandelbrot_application::RecreateSwapchain(const uint32_t width,
                                                        const uint32_t height) {
  m_SwapchainExtent.width = width;
  m_SwapchainExtent.height = height;

  if (!m_window.has_value()) {
    return;
  }

  while (m_SwapchainExtent.width == 0 || m_SwapchainExtent.height == 0) {
    m_window->poll([this](const event &polled) { OnEvent(polled); });
    const auto [windowWidth, windowHeight] = m_window->get_size();

    m_SwapchainExtent.width = windowWidth;
    m_SwapchainExtent.height = windowHeight;
  }

  VK_CHECK(vkDeviceWaitIdle(m_Context.device()));
  CleanupSwapchain();
  CreateSwapchain();
  RecordGraphicsCommandBuffers();
}

void realtime_mandelbrot_application::CleanupSwapchain() {
  vkDestroyRenderPass(m_Context.device(), m_SwapchainRenderPass, nullptr);

  for (uint32_t i = 0; i < m_ImageCount; ++i) {
    vkDestroyFramebuffer(m_Context.device(), m_SwapchainFramebuffers[i],
                         nullptr);

    vkDestroyImageView(m_Context.device(), m_SwapchainImageViews[i], nullptr);

    vkDestroyFence(m_Context.device(), m_InFlightFences[i], nullptr);
  }

  m_ImagesInFlight.clear();
  for (uint32_t i = 0; i < m_MaxFramesInFlight; ++i) {
    if (m_Semaphores.PresentComplete.empty())
      return;

    if (m_Semaphores.PresentComplete[i])
      vkDestroySemaphore(m_Context.device(), m_Semaphores.PresentComplete[i],
                         nullptr);

    if (m_Semaphores.RenderComplete[i])
      vkDestroySemaphore(m_Context.device(), m_Semaphores.RenderComplete[i],
                         nullptr);
  }

  vkDestroySwapchainKHR(m_Context.device(), m_Swapchain, nullptr);
}
