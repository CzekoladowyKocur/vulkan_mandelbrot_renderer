#pragma once
#include "include/Core.hpp"
#include "include/Input.hpp"
#include "include/function_ref.hpp"
#include <expected>
#include <memory>
#include <system_error>
#include <vulkan/vulkan.h>

enum class EEventType : std::uint8_t {
  None = 0,
  WindowClose,
  WindowResize,
  WindowMinimize,
  KeyPressed,
  KeyReleased,
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

class KeyEvent : public Event {
public:
  KeyEvent(const EEventType type, const KeyCode keyCode)
      : Event(type), m_KeyCode(keyCode) {}

  virtual ~KeyEvent() = default;

  [[nodiscard]]
  KeyCode GetKeyCode() const {
    return m_KeyCode;
  }

private:
  KeyCode m_KeyCode;
};

class KeyPressedEvent : public KeyEvent {
public:
  explicit KeyPressedEvent(const KeyCode keyCode)
      : KeyEvent(EEventType::KeyPressed, keyCode) {}
};

class KeyReleasedEvent : public KeyEvent {
public:
  explicit KeyReleasedEvent(const KeyCode keyCode)
      : KeyEvent(EEventType::KeyReleased, keyCode) {}
};

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

  void poll(const function_ref<void(Event &)> callback) noexcept;

private:
  explicit window(window_props &&props);

  [[nodiscard]]
  std::expected<void, std::error_code> initialize() noexcept;

  struct detail;
  std::unique_ptr<detail> m_detail;
};
