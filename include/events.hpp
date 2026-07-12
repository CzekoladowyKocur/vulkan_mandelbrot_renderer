#pragma once
#include "include/input.hpp"
#include <cstdint>
#include <variant>

struct window_close_event final {};

struct window_resize_event final {
  std::uint32_t width{};
  std::uint32_t height{};
};

struct window_minimize_event final {};

struct key_pressed_event final {
  key_code code{};
};

struct key_released_event final {
  key_code code{};
};

using event =
    std::variant<window_close_event, window_resize_event, window_minimize_event,
                 key_pressed_event, key_released_event>;

template <typename... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
