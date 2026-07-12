#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <print>
#include <vulkan/vulkan.h>

#include "include/Core.hpp"

#include "include/Application.hpp"

#undef APIENTRY
int __stdcall WINAPI wWinMain(_In_ HINSTANCE hInstance,
                              _In_opt_ HINSTANCE hPrevInstance,
                              _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)hInstance;

  ::ShowWindow(::GetConsoleWindow(), nShowCmd != 0 ? SW_SHOW : SW_HIDE);

  auto application{new VulkanApp(VulkanApp::ERenderMethod::Compute)};
  if (application->Initialize()) {
    if (application->Run()) {

    } else {
      std::println("Failed to run application properly");
      delete application;
      return EXIT_FAILURE;
    }

    if (application->Shutdown()) {
      /* Everything went well up to this point.. weird.. */
    } else {
      std::println("Failed to shutdown application properly");
      delete application;
      return EXIT_FAILURE;
    }
  } else {
    std::println("Failed to initialize application");
    delete application;
    return EXIT_FAILURE;
  } /* Application Initialize */

  std::println("Shutting down. . .");
  delete application;
  return EXIT_SUCCESS;
}
