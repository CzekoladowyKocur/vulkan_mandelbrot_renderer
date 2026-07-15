#pragma once
#include <cstdint>

enum class app_mode : std::uint8_t { compute, graphics, _default = graphics };

[[nodiscard]] int vulkan_app_main(const app_mode mode);
