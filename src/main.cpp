#include <cstdlib>
#include <print>
#include <string_view>

#include "include/app_main.hpp"
#include "include/compute_mandelbrot_application.hpp"
#include "include/realtime_mandelbrot_application.hpp"

int vulkan_app_main(const bool compute) {
  if (compute) {
    if (const auto result{run_compute_mandelbrot_application("mandelbrot.png")};
        !result) {
      std::println("Failed to render mandelbrot image: {}",
                   result.error().message());
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  realtime_mandelbrot_application application{};

  if (const auto result{application.run()}; !result) {
    std::println("Failed to run application: {}", result.error().message());
    return EXIT_FAILURE;
  }

  std::println("Shutting down. . .");
  return EXIT_SUCCESS;
}

#ifndef _WIN32

int main(const int argc, const char *const *const argv) {
  try {
    bool compute{false};
    for (int i{1}; i < argc; ++i) {
      if (std::string_view{argv[i]} == "--compute") {
        compute = true;
      }
    }

    return vulkan_app_main(compute);
  } catch (...) {
    return EXIT_FAILURE;
  }
}

#endif
