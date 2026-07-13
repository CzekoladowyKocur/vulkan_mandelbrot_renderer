#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <print>
#include <string_view>
#include <vulkan/vulkan.h>

#include "include/compute_mandelbrot_application.hpp"
#include "include/realtime_mandelbrot_application.hpp"

[[nodiscard]] static bool parse_compute_flag() noexcept {
  int argument_count{0};
  wchar_t **const arguments{
      ::CommandLineToArgvW(::GetCommandLineW(), &argument_count)};
  if (arguments == nullptr) {
    return false;
  }

  bool compute{false};
  for (int i{1}; i < argument_count; ++i) {
    if (std::wstring_view{arguments[i]} == L"--compute") {
      compute = true;
    }
  }

  ::LocalFree(static_cast<HLOCAL>(static_cast<void *>(arguments)));
  return compute;
}

[[nodiscard]] static int vulkan_app_main(const bool compute) {
  if (compute) {
    if (const auto result{run_compute_mandelbrot_application("mandelbrot.png")};
        !result) {
      std::println("Failed to render mandelbrot image: {}",
                   result.error().message());
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  const auto application{std::make_unique<realtime_mandelbrot_application>()};

  if (!application->Initialize()) {
    std::println("Failed to initialize application");
    return EXIT_FAILURE;
  }

  if (!application->Run()) {
    std::println("Failed to run application properly");
    return EXIT_FAILURE;
  }

  if (!application->Shutdown()) {
    std::println("Failed to shutdown application properly");
    return EXIT_FAILURE;
  }

  std::println("Shutting down. . .");
  return EXIT_SUCCESS;
}

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

  const int result{vulkan_app_main(parse_compute_flag())};

  ::UnregisterClassA(g_window_class_name, hInstance);
  return result;
}
