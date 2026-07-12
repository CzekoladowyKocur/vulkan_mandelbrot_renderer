#include "include/Input.h"
#include "include/Application.h"

bool Input::IsKeyPressed(const KeyCode keyCode)
{
	return VulkanApp::GetInstance()->m_Window->KeyPressed(keyCode);
}