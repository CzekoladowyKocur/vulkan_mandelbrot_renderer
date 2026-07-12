#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

/* Object file for library builds */
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma warning(push, 0)
#include <lodepng.cpp>
#pragma warning(pop)
/* TODO: Add Imgui */

#if defined(__clang__)
#pragma clang diagnostic pop
#endif