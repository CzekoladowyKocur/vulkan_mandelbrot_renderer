#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <print>
#include <string_view>
#include <vulkan/vulkan.h>

#include "include/Application.hpp"

[[nodiscard]] static VulkanApp::ERenderMethod parse_render_method() noexcept {
  int argument_count{0};
  wchar_t **const arguments{
      ::CommandLineToArgvW(::GetCommandLineW(), &argument_count)};
  if (arguments == nullptr) {
    return VulkanApp::ERenderMethod::Graphics;
  }

  auto render_method{VulkanApp::ERenderMethod::Graphics};
  for (int i{1}; i < argument_count; ++i) {
    if (std::wstring_view{arguments[i]} == L"--compute") {
      render_method = VulkanApp::ERenderMethod::Compute;
    }
  }

  ::LocalFree(static_cast<HLOCAL>(static_cast<void *>(arguments)));
  return render_method;
}

[[nodiscard]] static int
vulkan_app_main(const VulkanApp::ERenderMethod render_method) {
  const auto application{std::make_unique<VulkanApp>(render_method)};

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

  const int result{vulkan_app_main(parse_render_method())};

  ::UnregisterClassA(g_window_class_name, hInstance);
  return result;
}
