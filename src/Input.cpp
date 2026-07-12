#include "include/Input.hpp"
#include "include/Application.hpp"

bool Input::IsKeyPressed(const KeyCode keyCode) noexcept {
  return VulkanApp::GetInstance()->m_Window->KeyPressed(keyCode);
}
