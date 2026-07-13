#pragma once
#include <expected>
#include <filesystem>
#include <system_error>

[[nodiscard]] std::expected<void, std::error_code>
run_compute_mandelbrot_application(const std::filesystem::path &output_path);
