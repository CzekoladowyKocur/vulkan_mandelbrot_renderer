#pragma once
#include <array>
#include <cstdint>
#include <utility>

enum class key_code : std::uint8_t {
  none,

  backspace,
  tab,
  enter,
  escape,
  space,

  shift,
  control,
  alt,
  left_shift,
  right_shift,
  left_control,
  right_control,
  left_alt,
  right_alt,
  left_super,
  right_super,
  menu,

  caps_lock,
  pause,
  print_screen,
  num_lock,
  scroll_lock,

  page_up,
  page_down,
  end,
  home,
  insert,
  del,

  left,
  up,
  right,
  down,

  a,
  b,
  c,
  d,
  e,
  f,
  g,
  h,
  i,
  j,
  k,
  l,
  m,
  n,
  o,
  p,
  q,
  r,
  s,
  t,
  u,
  v,
  w,
  x,
  y,
  z,

  numpad0,
  numpad1,
  numpad2,
  numpad3,
  numpad4,
  numpad5,
  numpad6,
  numpad7,
  numpad8,
  numpad9,
  multiply,
  add,
  subtract,
  decimal,
  divide,

  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  f9,
  f10,
  f11,
  f12,
  f13,
  f14,
  f15,
  f16,
  f17,
  f18,
  f19,
  f20,
  f21,
  f22,
  f23,
  f24,

  semicolon,
  plus,
  comma,
  minus,
  period,
  slash,
  grave,

  count,
};

class input final {
public:
  void set_key_state(const key_code code, const bool pressed) noexcept;

  [[nodiscard]]
  bool is_key_pressed(const key_code code) const noexcept;

private:
  std::array<bool, std::to_underlying(key_code::count)> m_key_states{};
};
