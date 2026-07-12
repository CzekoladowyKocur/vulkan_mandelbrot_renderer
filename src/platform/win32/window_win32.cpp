#include "include/window.hpp"

#include <Windows.h>

#include "include/VulkanTypes.hpp"

constexpr int g_callback_slot{0};

[[nodiscard]] static std::unexpected<std::error_code> get_last_error() noexcept;

static LRESULT CALLBACK win32_wndproc(const HWND hwnd, const UINT message,
                                      const WPARAM wParam,
                                      const LPARAM lParam) noexcept;

struct win32_window_state {
  std::uint32_t width{};
  std::uint32_t height{};
  std::string name{};

  HWND hwnd{};
  HINSTANCE hinstance{};
  bool maximized{};

  ~win32_window_state() {
    if (hwnd != nullptr) {
      ::DestroyWindow(hwnd);
    }
  }
};

struct window::detail final : win32_window_state {
  explicit detail(window_props &&props)
      : win32_window_state{.width{props.width},
                           .height{props.height},
                           .name{std::move(props.name)},
                           .maximized{props.maximized}} {}
};

std::expected<window, std::error_code>
window::create(window_props &&props) noexcept {
  try {
    window result{std::move(props)};

    if (const auto initialized{result.initialize()}; !initialized) {
      return std::unexpected{initialized.error()};
    }

    return result;
  } catch (const std::bad_alloc &) {
    return std::unexpected{std::make_error_code(std::errc::not_enough_memory)};
  }
}

window::window(window_props &&props)
    : m_detail{std::make_unique<detail>(std::move(props))} {}

window::window(window &&) noexcept = default;

window &window::operator=(window &&) noexcept = default;

window::~window() = default;

std::expected<void, std::error_code> window::initialize() noexcept {
  m_detail->hinstance = ::GetModuleHandleA(nullptr);

  const LPCSTR lpClassName{g_window_class_name};
  const LPCSTR lpWindowName{m_detail->name.c_str()};
  const DWORD dwExStyle{WS_EX_LEFT};
  const DWORD dwStyle{WS_OVERLAPPEDWINDOW};

  const int X{CW_USEDEFAULT};
  const int Y{CW_USEDEFAULT};
  const int nWidth{static_cast<int>(m_detail->width)};
  const int nHeight{static_cast<int>(m_detail->height)};

  const HWND hWndParent{nullptr};
  const HMENU hMenu{nullptr};
  const LPVOID lpParam{nullptr};

  m_detail->hwnd = ::CreateWindowExA(dwExStyle, lpClassName, lpWindowName,
                                     dwStyle, X, Y, nWidth, nHeight, hWndParent,
                                     hMenu, m_detail->hinstance, lpParam);

  if (m_detail->hwnd == nullptr) {
    return get_last_error();
  }

  const LONG_PTR dwNewUserData{reinterpret_cast<LONG_PTR>(
      static_cast<win32_window_state *>(m_detail.get()))};

  ::SetLastError(0u);
  if (::SetWindowLongPtrA(m_detail->hwnd, GWLP_USERDATA, dwNewUserData) == 0 &&
      ::GetLastError() != 0u) {
    return get_last_error();
  }

  const LONG_PTR dwNewWndProc{reinterpret_cast<LONG_PTR>(&win32_wndproc)};

  ::SetLastError(0u);
  if (::SetWindowLongPtrA(m_detail->hwnd, GWLP_WNDPROC, dwNewWndProc) == 0 &&
      ::GetLastError() != 0u) {
    return get_last_error();
  }

  const int nCmdShow{m_detail->maximized ? SW_SHOWMAXIMIZED : SW_SHOW};
  ::ShowWindow(m_detail->hwnd, nCmdShow);
  ::UpdateWindow(m_detail->hwnd);
  ::SetFocus(m_detail->hwnd);

  RECT client_rectangle;
  ::GetClientRect(m_detail->hwnd, &client_rectangle);

  m_detail->width = static_cast<std::uint32_t>(client_rectangle.right -
                                               client_rectangle.left);

  m_detail->height = static_cast<std::uint32_t>(client_rectangle.bottom -
                                                client_rectangle.top);

  return {};
}

std::expected<VkSurfaceKHR, std::error_code>
window::get_surface(const VkInstance instance) noexcept {
  const VkWin32SurfaceCreateInfoKHR surface_create_info{
      .sType{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR},
      .pNext{nullptr},
      .flags{0u},
      .hinstance{m_detail->hinstance},
      .hwnd{m_detail->hwnd},
  };

  VkSurfaceKHR surface{VK_NULL_HANDLE};
  const VkResult result{vkCreateWin32SurfaceKHR(instance, &surface_create_info,
                                                nullptr, &surface)};

  if (result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return surface;
}

void window::poll(const function_ref<void(const event &)> callback) noexcept {
  const LONG_PTR dwNewCallback{reinterpret_cast<LONG_PTR>(&callback)};
  ::SetWindowLongPtrA(m_detail->hwnd, g_callback_slot, dwNewCallback);

  const HWND hWndFilter{nullptr};
  const UINT wMsgFilterMin{0u};
  const UINT wMsgFilterMax{0u};
  const UINT wRemoveMsg{PM_REMOVE};

  MSG message;
  while (::PeekMessageA(&message, hWndFilter, wMsgFilterMin, wMsgFilterMax,
                        wRemoveMsg)) {
    ::TranslateMessage(&message);
    ::DispatchMessageA(&message);
  }

  ::SetWindowLongPtrA(m_detail->hwnd, g_callback_slot, LONG_PTR{0});
}

std::pair<std::uint32_t, std::uint32_t> window::get_size() const noexcept {
  return {m_detail->width, m_detail->height};
}

[[nodiscard]] static std::unexpected<std::error_code>
get_last_error() noexcept {
  return std::unexpected{std::error_code{static_cast<int>(::GetLastError()),
                                         std::system_category()}};
}

static LRESULT CALLBACK win32_wndproc(const HWND hwnd, const UINT message,
                                      const WPARAM wParam,
                                      const LPARAM lParam) noexcept {
  constexpr LRESULT event_handled{0};

  auto *const state{reinterpret_cast<win32_window_state *>(
      ::GetWindowLongPtrA(hwnd, GWLP_USERDATA))};

  auto *const callback{
      reinterpret_cast<const function_ref<void(const event &)> *>(
          ::GetWindowLongPtrA(hwnd, g_callback_slot))};

  const auto dispatch{[callback](const event &dispatched) noexcept {
    if (callback != nullptr) {
      (*callback)(dispatched);
    }
  }};

  switch (message) {
  case WM_CLOSE: {
    dispatch(window_close_event{});
    return event_handled;
  }

  case WM_DESTROY: {
    ::PostQuitMessage(0);
    return event_handled;
  }

  case WM_SIZE: {
    if (wParam == SIZE_MINIMIZED) {
      dispatch(window_minimize_event{});
      return event_handled;
    }

    RECT rectangle{};
    if (::GetClientRect(hwnd, &rectangle) == FALSE) {
      return event_handled;
    }

    const auto width{
        static_cast<std::uint32_t>(rectangle.right - rectangle.left)};

    const auto height{
        static_cast<std::uint32_t>(rectangle.bottom - rectangle.top)};

    if (state != nullptr) {
      state->width = width;
      state->height = height;
    }

    dispatch(window_resize_event{.width{width}, .height{height}});

    return event_handled;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    dispatch(key_pressed_event{.code{static_cast<key_code>(wParam)}});
    return event_handled;
  }

  case WM_KEYUP:
  case WM_SYSKEYUP: {
    dispatch(key_released_event{.code{static_cast<key_code>(wParam)}});
    return event_handled;
  }

  default:
    return ::DefWindowProcA(hwnd, message, wParam, lParam);
  }
}
