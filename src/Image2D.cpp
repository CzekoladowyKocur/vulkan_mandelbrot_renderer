#include <string_view>
#pragma warning(push, 0)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <stb_image.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#pragma warning(pop)

#include "include/Application.h"
#include "include/Image2D.h"

Image2D::Image2D(const std::string_view assetPath)
    : m_AssetPath(assetPath), m_Properties(), m_CPUData() {
  assert(Load());

  const VkDevice device = VulkanApp::GetInstance()->m_LogicalDevice;
  // const VkFormat format = m_Properties.ChannelCount == 3
  //                             ? VK_FORMAT_R8G8B8A8_UNORM
  //                              : VK_FORMAT_R8G8B8A8_UNORM;

  const VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
  uint32_t imageUsageFlags = 0;
  imageUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  VkImageCreateInfo imageCreateInfo;
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.extent.width = m_Properties.Width;
  imageCreateInfo.extent.height = m_Properties.Height;
  imageCreateInfo.extent.depth = 1u;
  imageCreateInfo.format = format;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.usage = imageUsageFlags;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED;
  imageCreateInfo.pQueueFamilyIndices = nullptr;
  imageCreateInfo.flags = 0;
  imageCreateInfo.pNext = nullptr;

  VK_CHECK(vkCreateImage(device, &imageCreateInfo, nullptr, &m_ImageHandle));

  /* Create a staging buffer */
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  {
    VkBufferCreateInfo stagingBufferCreateInfo;
    stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferCreateInfo.size = m_ImageMemorySpace;
    stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    stagingBufferCreateInfo.queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED;
    stagingBufferCreateInfo.pQueueFamilyIndices = nullptr;
    stagingBufferCreateInfo.flags = 0;
    stagingBufferCreateInfo.pNext = nullptr;

    VK_CHECK(vkCreateBuffer(device, &stagingBufferCreateInfo, nullptr,
                            &stagingBuffer));

    VkMemoryRequirements stagingBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer,
                                  &stagingBufferMemoryRequirements);

    VkMemoryAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = stagingBufferMemoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        VulkanApp::GetInstance()->RetrieveMemoryTypeIndex(
            stagingBufferMemoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    allocateInfo.pNext = nullptr;

    vkAllocateMemory(device, &allocateInfo, nullptr, &stagingBufferMemory);

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    void *data;
    vkMapMemory(device, stagingBufferMemory, 0, m_ImageMemorySpace, 0, &data);
    memcpy(data, m_CPUData.Data(), m_CPUData.Size());
    vkUnmapMemory(device, stagingBufferMemory);
  } /* Staging buffer */

  /* Image buffer */
  {
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, m_ImageHandle, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        VulkanApp::GetInstance()->RetrieveMemoryTypeIndex(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocateInfo.pNext = nullptr;

    VK_CHECK(vkAllocateMemory(device, &allocateInfo, nullptr, &m_ImageMemory));

    vkBindImageMemory(device, m_ImageHandle, m_ImageMemory, 0);
  }

  VkImageMemoryBarrier imageMemoryBarrier;
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.image = m_ImageHandle;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageMemoryBarrier.srcAccessMask = 0;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageMemoryBarrier.subresourceRange.layerCount = 1;
  imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
  imageMemoryBarrier.subresourceRange.levelCount = 1;
  imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
  imageMemoryBarrier.dstQueueFamilyIndex = 0;
  imageMemoryBarrier.srcQueueFamilyIndex = 0;
  imageMemoryBarrier.pNext = nullptr;

  VkBufferImageCopy imageBufferCopyRegion{};
  imageBufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBufferCopyRegion.imageSubresource.mipLevel = 0;
  imageBufferCopyRegion.imageSubresource.baseArrayLayer = 0;
  imageBufferCopyRegion.imageSubresource.layerCount = 1;
  imageBufferCopyRegion.imageExtent.width = m_Properties.Width;
  imageBufferCopyRegion.imageExtent.height = m_Properties.Height;
  imageBufferCopyRegion.imageExtent.depth = 1;
  imageBufferCopyRegion.imageOffset.x = 0;
  imageBufferCopyRegion.imageOffset.y = 0;
  imageBufferCopyRegion.imageOffset.z = 0;
  imageBufferCopyRegion.bufferImageHeight = 0;
  imageBufferCopyRegion.bufferOffset = 0;

  VkCommandBuffer commandBuffer =
      VulkanApp::GetInstance()->BeginRecordingSingleTimeUseCommands(false);

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &imageMemoryBarrier);

  vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_ImageHandle,
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

  /* Image view */
  VkImageViewCreateInfo imageViewCreateInfo;
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.image = m_ImageHandle;
  imageViewCreateInfo.format = format;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
  imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
  imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
  imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
  imageViewCreateInfo.flags = 0;
  imageViewCreateInfo.pNext = nullptr;

  VK_CHECK(vkCreateImageView(VulkanApp::GetInstance()->m_LogicalDevice,
                             &imageViewCreateInfo, nullptr, &m_ImageView));

  /* Sampler */
  VkSamplerCreateInfo samplerCreateInfo;
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerCreateInfo.maxAnisotropy = 1.0f;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = 100.0f;
  samplerCreateInfo.compareEnable = VK_FALSE;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_LESS;
  samplerCreateInfo.anisotropyEnable = VK_FALSE;
  samplerCreateInfo.unnormalizedCoordinates = 0;
  samplerCreateInfo.flags = 0;
  samplerCreateInfo.pNext = nullptr;

  VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_Sampler));

  /* Free staging buffer */
  vkFreeMemory(device, stagingBufferMemory, nullptr);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
}

Image2D::~Image2D() {
  const VkDevice device = VulkanApp::GetInstance()->m_LogicalDevice;

  vkDestroySampler(device, m_Sampler, nullptr);

  vkDestroyImageView(device, m_ImageView, nullptr);

  vkFreeMemory(device, m_ImageMemory, nullptr);

  vkDestroyImage(device, m_ImageHandle, nullptr);
}

VkImage Image2D::GetImageHandle() { return m_ImageHandle; }

VkImageView Image2D::GetImageView() { return m_ImageView; }

VkSampler Image2D::GetImageSampler() { return m_Sampler; }

bool Image2D::Load() {
  assert(std::filesystem::exists(m_AssetPath));
  constexpr uint64_t sizeOfPixel = sizeof(uint8_t) * 4;

  int32_t width, height, channelCount;
  stbi_uc *pixelData = stbi_load(m_AssetPath.string().c_str(), &width, &height,
                                 &channelCount, STBI_rgb_alpha);
  assert(pixelData);

  m_Properties = ImageProperties(static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height),
                                 static_cast<uint32_t>(channelCount));

  m_ImageMemorySpace =
      static_cast<VkDeviceSize>(static_cast<uint64_t>(width) *
                                static_cast<uint64_t>(height) * sizeOfPixel);

  m_CPUData.Set(static_cast<size_t>(m_ImageMemorySpace), pixelData);
  return true;
}
