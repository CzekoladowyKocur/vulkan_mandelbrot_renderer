#pragma once
#include "include/events.hpp"
#include "include/function_ref.hpp"
#include <expected>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

using VkInstance = struct VkInstance_T *;
using VkSurfaceKHR = struct VkSurfaceKHR_T *;

inline constexpr const char *g_window_class_name{"VulkanMandelbrotRenderer"};

struct window_props final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::string name{};
};

class window final {
public:
  [[nodiscard]]
  static std::expected<window, std::error_code>
  create(window_props &&props) noexcept;

  ~window();

  window(window &&) noexcept;
  window &operator=(window &&) noexcept;

  window(const window &) = delete;
  window &operator=(const window &) = delete;

  [[nodiscard]]
  std::expected<VkSurfaceKHR, std::error_code>
  get_surface(const VkInstance instance) noexcept;

  [[nodiscard]]
  std::pair<std::uint32_t, std::uint32_t> get_size() const noexcept;

  void poll(const function_ref<void(const event &)> callback) noexcept;

private:
  explicit window(window_props &&props);

  [[nodiscard]]
  std::expected<void, std::error_code> initialize() noexcept;

  struct detail;
  std::unique_ptr<detail> m_detail;
};
