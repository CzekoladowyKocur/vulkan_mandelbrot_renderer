#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <print>
#include <string>
#include <system_error>
#include <vector>
#include <vulkan/vulkan.h>

class vulkan_error_category final : public std::error_category {
public:
  [[nodiscard]]
  const char *name() const noexcept override {
    return "vulkan";
  }

  [[nodiscard]]
  std::string message(const int condition) const override {
    switch (static_cast<VkResult>(condition)) {
    case VK_SUCCESS:
      return "VK_SUCCESS";
    case VK_NOT_READY:
      return "VK_NOT_READY";
    case VK_TIMEOUT:
      return "VK_TIMEOUT";
    case VK_EVENT_SET:
      return "VK_EVENT_SET";
    case VK_EVENT_RESET:
      return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
      return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
      return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
      return "VK_ERROR_UNKNOWN";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
      return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
      return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
      return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_FRAGMENTATION:
      return "VK_ERROR_FRAGMENTATION";
    case VK_PIPELINE_COMPILE_REQUIRED:
      return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_NOT_PERMITTED_KHR:
      return "VK_ERROR_NOT_PERMITTED_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
      return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_INVALID_SHADER_NV:
      return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
      return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
      return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
#ifdef VK_EXT_present_timing
    case VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT:
      return "VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT";
#endif
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
      return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
      return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
      return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
      return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
      return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
      return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
      return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
#ifdef VK_EXT_shader_object
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
      return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
#endif
#ifdef VK_KHR_pipeline_binary
    case VK_PIPELINE_BINARY_MISSING_KHR:
      return "VK_PIPELINE_BINARY_MISSING_KHR";
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
      return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
#endif
    default:
      return "VkResult(" + std::to_string(condition) + ")";
    }
  }
};

[[nodiscard]] inline const std::error_category &vulkan_category() noexcept {
  static const vulkan_error_category category{};
  return category;
}

[[nodiscard]] inline std::unexpected<std::error_code>
make_vulkan_error(const VkResult result) noexcept {
  return std::unexpected{
      std::error_code{static_cast<int>(result), vulkan_category()}};
}

[[nodiscard]] inline std::uint32_t retrieve_memory_type_index(
    const VkPhysicalDevice physical_device,
    const std::uint32_t memory_type_bits,
    const VkMemoryPropertyFlags property_flags) noexcept {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

  for (std::uint32_t i{0u}; i < memory_properties.memoryTypeCount; ++i) {
    if ((memory_type_bits & (1u << i)) &&
        (memory_properties.memoryTypes[i].propertyFlags & property_flags) ==
            property_flags) {
      return i;
    }
  }

  assert(false);
  return 0;
}

struct single_time_command_context final {
  VkDevice device{VK_NULL_HANDLE};
  VkCommandPool command_pool{VK_NULL_HANDLE};
  VkQueue queue{VK_NULL_HANDLE};
};

[[nodiscard]] inline std::expected<VkCommandBuffer, std::error_code>
begin_single_time_commands(
    const single_time_command_context &context) noexcept {
  const VkCommandBufferAllocateInfo allocate_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
      .pNext{nullptr},
      .commandPool{context.command_pool},
      .level{VK_COMMAND_BUFFER_LEVEL_PRIMARY},
      .commandBufferCount{1u}};

  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  if (const VkResult result{vkAllocateCommandBuffers(
          context.device, &allocate_info, &command_buffer)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkCommandBufferBeginInfo begin_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO},
      .pNext{nullptr},
      .flags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT},
      .pInheritanceInfo{nullptr}};

  if (const VkResult result{vkBeginCommandBuffer(command_buffer, &begin_info)};
      result != VK_SUCCESS) {
    vkFreeCommandBuffers(context.device, context.command_pool, 1,
                         &command_buffer);
    return make_vulkan_error(result);
  }

  return command_buffer;
}

[[nodiscard]] inline std::expected<void, std::error_code>
end_single_time_commands(const single_time_command_context &context,
                         const VkCommandBuffer command_buffer) noexcept {
  struct submit_resources final {
    const single_time_command_context &context;
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    VkFence fence{VK_NULL_HANDLE};

    submit_resources(const submit_resources &) = delete;
    submit_resources &operator=(const submit_resources &) = delete;
    submit_resources(submit_resources &&) = delete;
    submit_resources &operator=(submit_resources &&) = delete;

    submit_resources(const single_time_command_context &in_context,
                     const VkCommandBuffer in_command_buffer) noexcept
        : context{in_context}, command_buffer{in_command_buffer} {}

    ~submit_resources() {
      vkDestroyFence(context.device, fence, nullptr);
      vkFreeCommandBuffers(context.device, context.command_pool, 1,
                           &command_buffer);
    }
  };

  submit_resources submit{context, command_buffer};

  if (const VkResult result{vkEndCommandBuffer(command_buffer)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkFenceCreateInfo fence_create_info{
      .sType{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}, .pNext{nullptr}, .flags{}};

  if (const VkResult result{vkCreateFence(context.device, &fence_create_info,
                                          nullptr, &submit.fence)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkSubmitInfo submit_info{.sType{VK_STRUCTURE_TYPE_SUBMIT_INFO},
                                 .pNext{nullptr},
                                 .waitSemaphoreCount{},
                                 .pWaitSemaphores{nullptr},
                                 .pWaitDstStageMask{nullptr},
                                 .commandBufferCount{1u},
                                 .pCommandBuffers{&command_buffer},
                                 .signalSemaphoreCount{},
                                 .pSignalSemaphores{nullptr}};

  if (const VkResult result{
          vkQueueSubmit(context.queue, 1, &submit_info, submit.fence)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (const VkResult result{vkWaitForFences(context.device, 1, &submit.fence,
                                            VK_FALSE, UINT64_MAX)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}

struct insert_image_memory_barrier_props final {
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  VkImage image{VK_NULL_HANDLE};
  VkAccessFlags src_access_mask{};
  VkAccessFlags dst_access_mask{};
  VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout new_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkPipelineStageFlags src_stage_mask{};
  VkPipelineStageFlags dst_stage_mask{};
  VkImageSubresourceRange subresource_range{};
};

inline void insert_image_memory_barrier(
    insert_image_memory_barrier_props &&props) noexcept {
  const VkImageMemoryBarrier image_memory_barrier{
      .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
      .pNext{nullptr},
      .srcAccessMask{props.src_access_mask},
      .dstAccessMask{props.dst_access_mask},
      .oldLayout{props.old_layout},
      .newLayout{props.new_layout},
      .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
      .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
      .image{props.image},
      .subresourceRange{props.subresource_range}};

  vkCmdPipelineBarrier(props.command_buffer, props.src_stage_mask,
                       props.dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1,
                       &image_memory_barrier);
}

[[nodiscard]] inline VkAccessFlags
source_access_mask_for_layout(const VkImageLayout layout) noexcept {
  switch (layout) {
  case VK_IMAGE_LAYOUT_UNDEFINED: {
    return 0;
  }
  case VK_IMAGE_LAYOUT_PREINITIALIZED: {
    return VK_ACCESS_HOST_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
    return VK_ACCESS_TRANSFER_READ_BIT;
  }
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
    return VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
    return VK_ACCESS_SHADER_READ_BIT;
  }
  default: {
    assert(false);
    return 0;
  }
  }
}

[[nodiscard]] inline VkAccessFlags
destination_access_mask_for_layout(const VkImageLayout layout) noexcept {
  switch (layout) {
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
    return VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
    return VK_ACCESS_TRANSFER_READ_BIT;
  }
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: {
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
    return VK_ACCESS_SHADER_READ_BIT;
  }
  default: {
    assert(false);
    return 0;
  }
  }
}

struct set_image_layout_props final {
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  VkImage image{VK_NULL_HANDLE};
  VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout new_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkPipelineStageFlags src_stage_mask{};
  VkPipelineStageFlags dst_stage_mask{};
};

inline void set_image_layout(set_image_layout_props &&props) noexcept {
  constexpr VkImageSubresourceRange subresource_range{
      .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
      .baseMipLevel{},
      .levelCount{1u},
      .baseArrayLayer{},
      .layerCount{1u}};

  VkAccessFlags src_access_mask{
      source_access_mask_for_layout(props.old_layout)};
  const VkAccessFlags dst_access_mask{
      destination_access_mask_for_layout(props.new_layout)};

  if (props.new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
      src_access_mask == 0) {
    src_access_mask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  }

  insert_image_memory_barrier({.command_buffer{props.command_buffer},
                               .image{props.image},
                               .src_access_mask{src_access_mask},
                               .dst_access_mask{dst_access_mask},
                               .old_layout{props.old_layout},
                               .new_layout{props.new_layout},
                               .src_stage_mask{props.src_stage_mask},
                               .dst_stage_mask{props.dst_stage_mask},
                               .subresource_range{subresource_range}});
}

#ifdef APP_DEBUG
#define VK_CHECK(x)                                                            \
  do {                                                                         \
    if (const VkResult vk_check_result{(x)}; vk_check_result != VK_SUCCESS) {  \
      std::println(                                                            \
          stderr, "{} failed: {} [{}:{}]", #x,                                 \
          vulkan_category().message(static_cast<int>(vk_check_result)),        \
          __FILE__, __LINE__);                                                 \
      assert(false);                                                           \
    }                                                                          \
  } while (false)
#else
#define VK_CHECK(x) x
#endif

struct VulkanBuffer {
  VkBuffer Handle = VK_NULL_HANDLE;
  VkDeviceMemory DeviceMemory = VK_NULL_HANDLE;
  std::vector<std::byte> CPUData;
};
