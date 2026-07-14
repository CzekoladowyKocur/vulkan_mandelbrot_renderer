#include <cstdlib>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include "app_main.hpp"
#include "window.hpp"

namespace {
[[nodiscard]] app_mode parse_flags() noexcept {
  int argument_count{0};
  app_mode mode{app_mode::_default};

  wchar_t **const arguments{
      ::CommandLineToArgvW(::GetCommandLineW(), &argument_count)};

  if (arguments == nullptr) {
    return mode;
  }

  for (int i{1}; i < argument_count; ++i) {
    if (std::wstring_view{arguments[i]} == L"--compute") {
      mode = app_mode::compute;
    }
  }

  ::LocalFree(static_cast<HLOCAL>(static_cast<void *>(arguments)));
  return mode;
}
} // namespace

#undef APIENTRY
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
  (void)hPrevInstance;
  (void)lpCmdLine;

  ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  const WNDCLASSEXA window_class{
      .cbSize{sizeof(window_class)},
      .style{0u},
      .lpfnWndProc{&::DefWindowProcA},
      .cbClsExtra{0},
      .cbWndExtra{sizeof(LONG_PTR)},
      .hInstance{hInstance},
      .hIcon{::LoadIconA(nullptr, reinterpret_cast<LPCSTR>(IDI_APPLICATION))},
      .hCursor{::LoadCursorA(nullptr, reinterpret_cast<LPCSTR>(IDC_ARROW))},
      .hbrBackground{reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH))},
      .lpszMenuName{nullptr},
      .lpszClassName{g_window_class_name},
      .hIconSm{nullptr},
  };

  if (::RegisterClassExA(&window_class) == 0) {
    ::MessageBoxExA(nullptr, "Failed to register window class!", nullptr,
                    MB_ICONERROR, LANG_SYSTEM_DEFAULT);

    return EXIT_FAILURE;
  }

  ::ShowWindow(::GetConsoleWindow(), nShowCmd != 0 ? SW_SHOW : SW_HIDE);

  const int result{vulkan_app_main(parse_flags())};

  ::UnregisterClassA(g_window_class_name, hInstance);
  return result;
}
