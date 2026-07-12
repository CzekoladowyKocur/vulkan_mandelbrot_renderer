#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <print>
#include <vulkan/vulkan.h>

#include "include/Core.hpp"

#include "include/Application.hpp"

static int vulkan_app_main() {
  auto application{new VulkanApp(VulkanApp::ERenderMethod::Compute)};

  if (!application->Initialize()) {
    std::println("Failed to initialize application");
    delete application;
    return EXIT_FAILURE;
  }

  if (!application->Run()) {
    std::println("Failed to run application properly");
    delete application;
    return EXIT_FAILURE;
  }

  if (!application->Shutdown()) {
    std::println("Failed to shutdown application properly");
    delete application;
    return EXIT_FAILURE;
  }

  std::println("Shutting down. . .");
  delete application;
  return EXIT_SUCCESS;
}

#undef APIENTRY
int __stdcall WINAPI wWinMain(_In_ HINSTANCE hInstance,
                              _In_opt_ HINSTANCE hPrevInstance,
                              _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
  (void)hPrevInstance;
  (void)lpCmdLine;

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

  const int result{vulkan_app_main()};

  ::UnregisterClassA(g_window_class_name, hInstance);
  return result;
}
