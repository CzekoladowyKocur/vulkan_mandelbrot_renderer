#pragma once
#include "include/VulkanTypes.hpp"
#include "include/image_2d.hpp"

struct texture_2d_sampler_props final {
  VkFilter filter{VK_FILTER_LINEAR};
  VkSamplerAddressMode address_mode{VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT};
};

struct texture_2d_props final {
  const image_2d &image;
  VkDevice device{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  texture_2d_sampler_props sampler{};
};

class texture_2d final {
public:
  [[nodiscard]]
  static std::expected<texture_2d, std::error_code>
  create(const texture_2d_props &props) noexcept;

  texture_2d(const texture_2d &) = delete;
  texture_2d &operator=(const texture_2d &) = delete;

  texture_2d(texture_2d &&other) noexcept;
  texture_2d &operator=(texture_2d &&other) noexcept;

  ~texture_2d();

  [[nodiscard]] VkImage GetImageHandle() const noexcept;
  [[nodiscard]] VkImageView GetImageView() const noexcept;
  [[nodiscard]] VkSampler GetImageSampler() const noexcept;

private:
  texture_2d(const std::uint32_t width, const std::uint32_t height,
             const VkDevice device) noexcept;

  [[nodiscard]] std::expected<void, std::error_code>
  initialize(const image_2d &image, const VkPhysicalDevice physical_device,
             const texture_2d_sampler_props &sampler);

  void destroy() noexcept;

private:
  std::uint32_t m_width{};
  std::uint32_t m_height{};

  VkDevice m_device{VK_NULL_HANDLE};

  VkImage m_ImageHandle{VK_NULL_HANDLE};
  VkDeviceMemory m_ImageMemory{VK_NULL_HANDLE};

  VkImageView m_ImageView{VK_NULL_HANDLE};
  VkSampler m_Sampler{VK_NULL_HANDLE};
};
