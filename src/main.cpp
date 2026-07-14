#include <cstdlib>
#include <print>
#include <string_view>

#include "app_main.hpp"
#include "compute_mandelbrot_application.hpp"
#include "realtime_mandelbrot_application.hpp"

int vulkan_app_main(const app_mode mode) {
  switch (mode) {
  case app_mode::compute: {
    if (const auto result{run_compute_mandelbrot_application("mandelbrot.png")};
        !result) {
      std::println("Failed to render mandelbrot image: {}",
                   result.error().message());

      return EXIT_FAILURE;
    }

    break;
  }

  case app_mode::graphics: {
    realtime_mandelbrot_application application{};

    if (const auto result{application.run()}; !result) {
      std::println("Failed to run application: {}", result.error().message());
      return EXIT_FAILURE;
    }

    break;
  }
  }

  std::println("Shutting down. . .");
  return EXIT_SUCCESS;
}

#ifndef _WIN32

int main(const int argc, const char *const *const argv) {
  try {
    app_mode mode{app_mode::_default};
    for (int i{1}; i < argc; ++i) {
      if (std::string_view{argv[i]} == "--compute") {
        mode = app_mode::compute;
      }
    }

    return vulkan_app_main(mode);
  } catch (...) {
    return EXIT_FAILURE;
  }
}

#endif
