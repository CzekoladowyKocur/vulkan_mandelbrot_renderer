#include "include/Input.hpp"

void Input::SetKeyState(const KeyCode keyCode, const bool pressed) noexcept {
  if (keyCode < static_cast<KeyCode>(Key::KEYCODES_END)) {
    m_KeyStates[static_cast<std::size_t>(keyCode)] =
        static_cast<uint8_t>(pressed);
  }
}

bool Input::IsKeyPressed(const KeyCode keyCode) const noexcept {
  return static_cast<bool>(m_KeyStates[static_cast<std::size_t>(keyCode)]);
}
