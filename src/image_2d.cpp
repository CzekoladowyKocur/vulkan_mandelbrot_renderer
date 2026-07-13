#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include <stb_image.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "include/image_2d.hpp"

std::expected<image_2d, std::error_code>
image_2d::create(const std::filesystem::path &path) noexcept {
  try {
    if (!std::filesystem::exists(path)) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    std::int32_t width{};
    std::int32_t height{};
    std::int32_t channel_count{};

    stbi_uc *const pixel_data{stbi_load(path.string().c_str(), &width, &height,
                                        &channel_count, STBI_rgb_alpha)};
    if (!pixel_data) {
      return std::unexpected(std::make_error_code(std::errc::io_error));
    }

    constexpr auto size_of_pixel{4uz};
    const auto byte_count{static_cast<std::size_t>(width) *
                          static_cast<std::size_t>(height) * size_of_pixel};

    const auto *const pixel_bytes{
        reinterpret_cast<const std::byte *>(pixel_data)};

    image_2d image{.width{static_cast<std::uint32_t>(width)},
                   .height{static_cast<std::uint32_t>(height)},
                   .pixels{pixel_bytes, pixel_bytes + byte_count}};

    stbi_image_free(pixel_data);

    return image;
  } catch (...) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}
