#pragma once
#include "include/Core.h"
#include "include/Input.h"

enum class EEventType : std::uint8_t {
  None = 0,
  WindowClose,
  WindowResize,
  WindowMinimize,
  KeyPressed,
  MouseButtonPressed,
};

class Event {
public:
  Event(const EEventType type) : m_Flags(type) {}

  ~Event() = default;

  [[nodiscard]]
  virtual EEventType GetEventType() const {
    return m_Flags;
  }

protected:
  EEventType m_Flags;
};

class WindowCloseEvent : public Event {
public:
  WindowCloseEvent() : Event(EEventType::WindowClose) {}

  virtual ~WindowCloseEvent() = default;

private:
};

class WindowResizeEvent : public Event {
public:
  WindowResizeEvent(const uint32_t width, const uint32_t height)
      : Event(EEventType::WindowResize), m_Width(width), m_Height(height) {}

  virtual ~WindowResizeEvent() = default;

  [[nodiscard]]
  const std::pair<uint32_t, uint32_t> GetSize() const {
    return {m_Width, m_Height};
  }

private:
  uint32_t m_Width, m_Height;
};

using WindowCallbackFunction = std::function<void(Event &)>;
struct WindowProperties {
  uint32_t Width, Height;
  bool ShowCMD;
  WindowCallbackFunction CallbackFunction;

  WindowProperties(const uint32_t width, const uint32_t height,
                   const bool showCMD, WindowCallbackFunction function)
      : Width(width), Height(height), ShowCMD(showCMD),
        CallbackFunction(std::move(function)) {}
};

class Window {
public:
  Window(const HINSTANCE hInstance,
         WindowProperties &&windowProperties) noexcept;
  ~Window();

  void PollEvents();
  bool KeyPressed(const KeyCode keyCode);

  [[nodiscard]]
  const std::pair<uint32_t, uint32_t> GetSize() const;

  [[nodiscard]]
  const std::pair<HWND, HINSTANCE> GetInternalState() const;

private:
  HWND m_Handle{nullptr};
  HINSTANCE m_HInstance;
  WindowProperties m_WindowProperties;

  std::array<uint8_t, static_cast<std::size_t>(Key::KEYCODES_END)> m_KeyStates;

private:
  friend LRESULT CALLBACK Win32ProcedureEventFunctionCallback(HWND hwnd,
                                                              UINT message,
                                                              WPARAM wParam,
                                                              LPARAM lParam);
};
