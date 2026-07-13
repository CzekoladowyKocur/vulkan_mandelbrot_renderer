#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>
#include <vector>

struct image_2d final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::byte> pixels{};

  [[nodiscard]]
  static std::expected<image_2d, std::error_code>
  create(const std::filesystem::path &path) noexcept;
};
