#include "include/texture_2d.hpp"
#include "include/Application.hpp"
#include <cstring>
#include <memory>
#include <utility>

std::expected<texture_2d, std::error_code>
texture_2d::create(const texture_2d_props &props) noexcept {
  try {
    if (props.image.pixels.empty()) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    texture_2d texture{};
    if (const auto result{texture.initialize(props)}; !result) {
      return std::unexpected(result.error());
    }

    return texture;
  } catch (...) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}

std::expected<void, std::error_code>
texture_2d::initialize(const texture_2d_props &props) {
  m_width = props.image.width;
  m_height = props.image.height;

  const VkDevice device{VulkanApp::GetInstance()->m_LogicalDevice};
  constexpr VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
  const VkDeviceSize pixelByteCount{props.image.pixels.size()};

  struct staging_resources final {
    VkDevice device{VK_NULL_HANDLE};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};

    staging_resources(const staging_resources &) = delete;
    staging_resources &operator=(const staging_resources &) = delete;
    staging_resources(staging_resources &&) = delete;
    staging_resources &operator=(staging_resources &&) = delete;

    explicit staging_resources(const VkDevice in_device) : device{in_device} {}

    ~staging_resources() {
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
    }
  };

  staging_resources staging{device};

  constexpr VkImageUsageFlags imageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT};

  const VkImageCreateInfo imageCreateInfo{
      .sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .imageType{VK_IMAGE_TYPE_2D},
      .format{format},
      .extent{.width{m_width}, .height{m_height}, .depth{1u}},
      .mipLevels{1},
      .arrayLayers{1},
      .samples{VK_SAMPLE_COUNT_1_BIT},
      .tiling{VK_IMAGE_TILING_OPTIMAL},
      .usage{imageUsageFlags},
      .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
      .queueFamilyIndexCount{VK_QUEUE_FAMILY_IGNORED},
      .pQueueFamilyIndices{nullptr},
      .initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}};

  if (const VkResult result{
          vkCreateImage(device, &imageCreateInfo, nullptr, &m_ImageHandle)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  {
    const VkBufferCreateInfo stagingBufferCreateInfo{
        .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .size{pixelByteCount},
        .usage{VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
        .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
        .queueFamilyIndexCount{VK_QUEUE_FAMILY_IGNORED},
        .pQueueFamilyIndices{nullptr}};

    if (const VkResult result{vkCreateBuffer(device, &stagingBufferCreateInfo,
                                             nullptr, &staging.buffer)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    VkMemoryRequirements stagingBufferMemoryRequirements{};
    vkGetBufferMemoryRequirements(device, staging.buffer,
                                  &stagingBufferMemoryRequirements);

    const VkMemoryAllocateInfo allocateInfo{
        .sType{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO},
        .pNext{nullptr},
        .allocationSize{stagingBufferMemoryRequirements.size},
        .memoryTypeIndex{VulkanApp::GetInstance()->RetrieveMemoryTypeIndex(
            stagingBufferMemoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)}};

    if (const VkResult result{
            vkAllocateMemory(device, &allocateInfo, nullptr, &staging.memory)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    if (const VkResult result{
            vkBindBufferMemory(device, staging.buffer, staging.memory, 0)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    void *data{nullptr};
    if (const VkResult result{
            vkMapMemory(device, staging.memory, 0, pixelByteCount, 0, &data)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    std::memcpy(data, props.image.pixels.data(), props.image.pixels.size());
    vkUnmapMemory(device, staging.memory);
  }

  {
    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device, m_ImageHandle, &memoryRequirements);

    const VkMemoryAllocateInfo allocateInfo{
        .sType{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO},
        .pNext{nullptr},
        .allocationSize{memoryRequirements.size},
        .memoryTypeIndex{VulkanApp::GetInstance()->RetrieveMemoryTypeIndex(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)}};

    if (const VkResult result{
            vkAllocateMemory(device, &allocateInfo, nullptr, &m_ImageMemory)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    if (const VkResult result{
            vkBindImageMemory(device, m_ImageHandle, m_ImageMemory, 0)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }
  }

  const VkImageMemoryBarrier imageMemoryBarrier{
      .sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
      .pNext{nullptr},
      .srcAccessMask{},
      .dstAccessMask{VK_ACCESS_TRANSFER_WRITE_BIT},
      .oldLayout{VK_IMAGE_LAYOUT_UNDEFINED},
      .newLayout{VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      .srcQueueFamilyIndex{},
      .dstQueueFamilyIndex{},
      .image{m_ImageHandle},
      .subresourceRange{.aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{},
                        .levelCount{1},
                        .baseArrayLayer{},
                        .layerCount{1}}};

  const VkBufferImageCopy imageBufferCopyRegion{
      .bufferOffset{},
      .bufferRowLength{},
      .bufferImageHeight{},
      .imageSubresource{.aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .mipLevel{},
                        .baseArrayLayer{},
                        .layerCount{1}},
      .imageOffset{.x{}, .y{}, .z{}},
      .imageExtent{.width{m_width}, .height{m_height}, .depth{1}}};

  VkCommandBuffer commandBuffer{
      VulkanApp::GetInstance()->BeginRecordingSingleTimeUseCommands(false)};

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);

  vkCmdCopyBufferToImage(commandBuffer, staging.buffer, m_ImageHandle,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &imageBufferCopyRegion);

  VulkanApp::GetInstance()->InsertImageMemoryBarrier(
      commandBuffer, m_ImageHandle, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, imageMemoryBarrier.subresourceRange);

  VulkanApp::GetInstance()->InsertImageMemoryBarrier(
      commandBuffer, m_ImageHandle, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, imageMemoryBarrier.subresourceRange);

  VulkanApp::GetInstance()->EndRecordingSingleTimeUseCommands(commandBuffer,
                                                              false);

  const VkImageViewCreateInfo imageViewCreateInfo{
      .sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .image{m_ImageHandle},
      .viewType{VK_IMAGE_VIEW_TYPE_2D},
      .format{format},
      .components{.r{VK_COMPONENT_SWIZZLE_R},
                  .g{VK_COMPONENT_SWIZZLE_G},
                  .b{VK_COMPONENT_SWIZZLE_B},
                  .a{VK_COMPONENT_SWIZZLE_A}},
      .subresourceRange{.aspectMask{VK_IMAGE_ASPECT_COLOR_BIT},
                        .baseMipLevel{},
                        .levelCount{1},
                        .baseArrayLayer{},
                        .layerCount{1}}};

  if (const VkResult result{vkCreateImageView(device, &imageViewCreateInfo,
                                              nullptr, &m_ImageView)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkSamplerCreateInfo samplerCreateInfo{
      .sType{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .magFilter{props.sampler.filter},
      .minFilter{props.sampler.filter},
      .mipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR},
      .addressModeU{props.sampler.address_mode},
      .addressModeV{props.sampler.address_mode},
      .addressModeW{props.sampler.address_mode},
      .mipLodBias{0.0f},
      .anisotropyEnable{VK_FALSE},
      .maxAnisotropy{1.0f},
      .compareEnable{VK_FALSE},
      .compareOp{VK_COMPARE_OP_LESS},
      .minLod{0.0f},
      .maxLod{100.0f},
      .borderColor{VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE},
      .unnormalizedCoordinates{VK_FALSE}};

  if (const VkResult result{
          vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_Sampler)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}

texture_2d::texture_2d(texture_2d &&other) noexcept
    : m_width{other.m_width}, m_height{other.m_height},
      m_ImageHandle{other.m_ImageHandle}, m_ImageMemory{other.m_ImageMemory},
      m_ImageView{other.m_ImageView}, m_Sampler{other.m_Sampler} {
  other.m_width = {};
  other.m_height = {};
  other.m_ImageHandle = VK_NULL_HANDLE;
  other.m_ImageMemory = VK_NULL_HANDLE;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;
}

texture_2d &texture_2d::operator=(texture_2d &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  m_width = other.m_width;
  m_height = other.m_height;
  m_ImageHandle = other.m_ImageHandle;
  m_ImageMemory = other.m_ImageMemory;
  m_ImageView = other.m_ImageView;
  m_Sampler = other.m_Sampler;

  other.m_width = {};
  other.m_height = {};
  other.m_ImageHandle = VK_NULL_HANDLE;
  other.m_ImageMemory = VK_NULL_HANDLE;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;

  return *this;
}

texture_2d::~texture_2d() { destroy(); }

void texture_2d::destroy() noexcept {
  const VkDevice device{VulkanApp::GetInstance()->m_LogicalDevice};

  vkDestroySampler(device, m_Sampler, nullptr);
  vkDestroyImageView(device, m_ImageView, nullptr);
  vkFreeMemory(device, m_ImageMemory, nullptr);
  vkDestroyImage(device, m_ImageHandle, nullptr);

  m_width = {};
  m_height = {};
  m_Sampler = VK_NULL_HANDLE;
  m_ImageView = VK_NULL_HANDLE;
  m_ImageMemory = VK_NULL_HANDLE;
  m_ImageHandle = VK_NULL_HANDLE;
}

VkImage texture_2d::GetImageHandle() const noexcept { return m_ImageHandle; }

VkImageView texture_2d::GetImageView() const noexcept { return m_ImageView; }

VkSampler texture_2d::GetImageSampler() const noexcept { return m_Sampler; }
