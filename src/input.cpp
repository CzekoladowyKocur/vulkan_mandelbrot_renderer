#include "input.hpp"
#include <utility>

void input::set_key_state(const key_code code, const bool pressed) noexcept {
  m_key_states[std::to_underlying(code)] = pressed;
}

bool input::is_key_pressed(const key_code code) const noexcept {
  return m_key_states[std::to_underlying(code)];
}
