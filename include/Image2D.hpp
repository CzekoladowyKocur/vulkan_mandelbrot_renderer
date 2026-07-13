#pragma once
#include "include/VulkanTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

struct image_data final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::byte> pixels{};

  [[nodiscard]]
  static std::expected<image_data, std::error_code>
  load_from_file(const std::filesystem::path &path) noexcept;
};

struct image_2d_sampler_props final {
  VkFilter filter{VK_FILTER_LINEAR};
  VkSamplerAddressMode address_mode{VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT};
};

struct image_2d_props final {
  const image_data &data;
  image_2d_sampler_props sampler{};
};

class image_2d final {
public:
  [[nodiscard]]
  static std::expected<image_2d, std::error_code>
  create(const image_2d_props &props) noexcept;

  image_2d(const image_2d &) = delete;
  image_2d &operator=(const image_2d &) = delete;

  image_2d(image_2d &&other) noexcept;
  image_2d &operator=(image_2d &&other) noexcept;

  ~image_2d();

  [[nodiscard]] VkImage GetImageHandle() const noexcept;
  [[nodiscard]] VkImageView GetImageView() const noexcept;
  [[nodiscard]] VkSampler GetImageSampler() const noexcept;

private:
  image_2d() = default;

  [[nodiscard]] std::expected<void, std::error_code>
  initialize(const image_2d_props &props);

  void destroy() noexcept;

private:
  std::uint32_t m_width{};
  std::uint32_t m_height{};

  VkImage m_ImageHandle{VK_NULL_HANDLE};
  VkDeviceMemory m_ImageMemory{VK_NULL_HANDLE};

  VkImageView m_ImageView{VK_NULL_HANDLE};
  VkSampler m_Sampler{VK_NULL_HANDLE};
};
