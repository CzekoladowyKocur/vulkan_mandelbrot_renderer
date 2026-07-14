#pragma once
#include "include/vulkan_types.hpp"
#include <cstdint>
#include <filesystem>
#include <span>

struct vulkan_context_props final {
  std::span<const char *const> instance_extensions{};
};

class vulkan_context final {
public:
  [[nodiscard]]
  static std::expected<vulkan_context, std::error_code>
  create(vulkan_context_props &&props) noexcept;

  vulkan_context() = default;

  vulkan_context(const vulkan_context &) = delete;
  vulkan_context &operator=(const vulkan_context &) = delete;

  vulkan_context(vulkan_context &&other) noexcept;
  vulkan_context &operator=(vulkan_context &&other) noexcept;

  ~vulkan_context();

  [[nodiscard]] VkInstance instance() const noexcept;
  [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
  [[nodiscard]] VkDevice device() const noexcept;

  [[nodiscard]] VkQueue graphics_queue() const noexcept;
  [[nodiscard]] VkQueue compute_queue() const noexcept;
  [[nodiscard]] std::uint32_t graphics_queue_family() const noexcept;
  [[nodiscard]] std::uint32_t compute_queue_family() const noexcept;

  [[nodiscard]] VkCommandPool graphics_command_pool() const noexcept;
  [[nodiscard]] VkCommandPool compute_command_pool() const noexcept;

private:
  [[nodiscard]] std::expected<void, std::error_code>
  initialize(const vulkan_context_props &props);

  [[nodiscard]] std::expected<void, std::error_code>
  create_instance(const std::span<const char *const> instance_extensions);

  [[nodiscard]] std::expected<void, std::error_code> select_physical_device();
  [[nodiscard]] std::expected<void, std::error_code>

  create_device(const bool presentation_required);
  [[nodiscard]] std::expected<void, std::error_code> create_command_pools();

  void destroy() noexcept;

private:
  VkInstance m_instance{VK_NULL_HANDLE};
#ifdef APP_DEBUG
  VkDebugReportCallbackEXT m_debug_report_callback{VK_NULL_HANDLE};
#endif

  VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
  std::uint32_t m_graphics_queue_family{};
  std::uint32_t m_compute_queue_family{};

  VkDevice m_device{VK_NULL_HANDLE};
  VkQueue m_graphics_queue{VK_NULL_HANDLE};
  VkQueue m_compute_queue{VK_NULL_HANDLE};

  VkCommandPool m_graphics_command_pool{VK_NULL_HANDLE};
  VkCommandPool m_compute_command_pool{VK_NULL_HANDLE};
};

[[nodiscard]] std::expected<VkShaderModule, std::error_code>
create_shader_module(const VkDevice device,
                     const std::filesystem::path &path) noexcept;
