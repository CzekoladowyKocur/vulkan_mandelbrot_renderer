#pragma once
#include "vulkan_types.hpp"
#include <cstddef>
#include <span>

struct vulkan_buffer_props final {
  VkDeviceSize size{};
  VkDevice device{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device{VK_NULL_HANDLE};
  VkBufferUsageFlags usage{};
  VkMemoryPropertyFlags memory_flags{};
};

class vulkan_buffer final {
public:
  [[nodiscard]]
  static std::expected<vulkan_buffer, std::error_code>
  create(vulkan_buffer_props &&props) noexcept;

  vulkan_buffer(const vulkan_buffer &) = delete;
  vulkan_buffer &operator=(const vulkan_buffer &) = delete;

  vulkan_buffer(vulkan_buffer &&other) noexcept;
  vulkan_buffer &operator=(vulkan_buffer &&other) noexcept;

  ~vulkan_buffer();

  [[nodiscard]] VkBuffer handle() const noexcept;
  [[nodiscard]] VkDeviceSize size() const noexcept;

  [[nodiscard]] std::expected<void, std::error_code>
  write(const std::span<const std::byte> data) noexcept;

  [[nodiscard]] std::expected<void, std::error_code>
  read(const std::span<std::byte> destination) const noexcept;

private:
  vulkan_buffer(const VkDeviceSize size, const VkDevice device) noexcept;

  [[nodiscard]] std::expected<void, std::error_code>
  initialize(const VkPhysicalDevice physical_device,
             const VkBufferUsageFlags usage,
             const VkMemoryPropertyFlags memory_flags);

  void destroy() noexcept;

private:
  VkDeviceSize m_size{};
  VkDevice m_device{VK_NULL_HANDLE};

  VkBuffer m_handle{VK_NULL_HANDLE};
  VkDeviceMemory m_memory{VK_NULL_HANDLE};
};

[[nodiscard]] std::expected<void, std::error_code>
copy_buffer(const single_time_command_context &commands, const VkBuffer source,
            const VkBuffer destination, const VkDeviceSize size) noexcept;
