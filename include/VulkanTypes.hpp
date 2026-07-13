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

inline void insert_image_memory_barrier(
    const VkCommandBuffer command_buffer, const VkImage image,
    const VkAccessFlags src_access_mask, const VkAccessFlags dst_access_mask,
    const VkImageLayout old_layout, const VkImageLayout new_layout,
    const VkPipelineStageFlags src_stage_mask,
    const VkPipelineStageFlags dst_stage_mask,
    const VkImageSubresourceRange &subresource_range) noexcept {
  const VkImageMemoryBarrier image_memory_barrier{
      .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
      .pNext{nullptr},
      .srcAccessMask{src_access_mask},
      .dstAccessMask{dst_access_mask},
      .oldLayout{old_layout},
      .newLayout{new_layout},
      .srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
      .dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
      .image{image},
      .subresourceRange{subresource_range}};

  vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0,
                       nullptr, 0, nullptr, 1, &image_memory_barrier);
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

inline void
set_image_layout(const VkCommandBuffer command_buffer, const VkImage image,
                 const VkImageLayout old_layout, const VkImageLayout new_layout,
                 const VkPipelineStageFlags src_stage_mask,
                 const VkPipelineStageFlags dst_stage_mask) noexcept {
  constexpr VkImageSubresourceRange subresource_range{
      .aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
      .baseMipLevel{},
      .levelCount{1},
      .baseArrayLayer{},
      .layerCount{1}};

  VkAccessFlags src_access_mask{source_access_mask_for_layout(old_layout)};
  const VkAccessFlags dst_access_mask{
      destination_access_mask_for_layout(new_layout)};

  if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
      src_access_mask == 0) {
    src_access_mask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  }

  insert_image_memory_barrier(
      command_buffer, image, src_access_mask, dst_access_mask, old_layout,
      new_layout, src_stage_mask, dst_stage_mask, subresource_range);
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
