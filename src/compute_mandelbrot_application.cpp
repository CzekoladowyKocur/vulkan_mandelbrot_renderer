#pragma warning(push, 0)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include <stb_image_write.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma warning(pop)

#include "include/compute_mandelbrot_application.hpp"
#include "include/vulkan_buffer.hpp"
#include "include/vulkan_context.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <vector>

namespace {

constexpr std::uint32_t render_width{3200u * 2u};
constexpr std::uint32_t render_height{2400u * 2u};
constexpr std::size_t compute_buffer_size{
    static_cast<std::size_t>(render_width) * render_height * sizeof(float) *
    4uz};
constexpr std::size_t rendered_image_size{
    static_cast<std::size_t>(render_width) * render_height *
    sizeof(std::uint8_t) * 4uz};
constexpr std::uint32_t workgroup_size{32u};

struct compute_pipeline_resources final {
  VkDevice device{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
  VkPipeline pipeline{VK_NULL_HANDLE};

  compute_pipeline_resources(const compute_pipeline_resources &) = delete;
  compute_pipeline_resources &
  operator=(const compute_pipeline_resources &) = delete;
  compute_pipeline_resources(compute_pipeline_resources &&) = delete;
  compute_pipeline_resources &operator=(compute_pipeline_resources &&) = delete;

  explicit compute_pipeline_resources(const VkDevice in_device) noexcept
      : device{in_device} {}

  ~compute_pipeline_resources() {
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
  }
};

[[nodiscard]] std::expected<void, std::error_code>
create_compute_pipeline(compute_pipeline_resources &resources,
                        const vulkan_buffer &storage_buffer) {
  const VkDescriptorSetLayoutBinding storage_buffer_binding{
      .binding{},
      .descriptorType{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
      .descriptorCount{1u},
      .stageFlags{VK_SHADER_STAGE_COMPUTE_BIT},
      .pImmutableSamplers{nullptr}};

  const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .bindingCount{1u},
      .pBindings{&storage_buffer_binding}};

  if (const VkResult result{vkCreateDescriptorSetLayout(
          resources.device, &descriptor_set_layout_create_info, nullptr,
          &resources.descriptor_set_layout)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .setLayoutCount{1u},
      .pSetLayouts{&resources.descriptor_set_layout},
      .pushConstantRangeCount{},
      .pPushConstantRanges{nullptr}};

  if (const VkResult result{
          vkCreatePipelineLayout(resources.device, &pipeline_layout_create_info,
                                 nullptr, &resources.pipeline_layout)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkDescriptorPoolSize pool_size{.type{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                                       .descriptorCount{1u}};

  const VkDescriptorPoolCreateInfo descriptor_pool_create_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .maxSets{1u},
      .poolSizeCount{1u},
      .pPoolSizes{&pool_size}};

  if (const VkResult result{
          vkCreateDescriptorPool(resources.device, &descriptor_pool_create_info,
                                 nullptr, &resources.descriptor_pool)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkDescriptorSetAllocateInfo descriptor_set_allocate_info{
      .sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO},
      .pNext{nullptr},
      .descriptorPool{resources.descriptor_pool},
      .descriptorSetCount{1u},
      .pSetLayouts{&resources.descriptor_set_layout}};

  if (const VkResult result{vkAllocateDescriptorSets(
          resources.device, &descriptor_set_allocate_info,
          &resources.descriptor_set)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkDescriptorBufferInfo buffer_info{
      .buffer{storage_buffer.handle()}, .offset{}, .range{compute_buffer_size}};

  const VkWriteDescriptorSet descriptor_set_write{
      .sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
      .pNext{nullptr},
      .dstSet{resources.descriptor_set},
      .dstBinding{},
      .dstArrayElement{},
      .descriptorCount{1u},
      .descriptorType{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
      .pImageInfo{nullptr},
      .pBufferInfo{&buffer_info},
      .pTexelBufferView{nullptr}};

  vkUpdateDescriptorSets(resources.device, 1, &descriptor_set_write, 0,
                         nullptr);

  const auto shader_module{create_shader_module(
      resources.device, "assets/shaders/computeShader.spv")};
  if (!shader_module) {
    std::println("Failed to create compute shader");
    return std::unexpected(shader_module.error());
  }

  const VkPipelineShaderStageCreateInfo shader_stage_create_info{
      .sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .stage{VK_SHADER_STAGE_COMPUTE_BIT},
      .module{*shader_module},
      .pName{"main"},
      .pSpecializationInfo{nullptr}};

  const VkComputePipelineCreateInfo pipeline_create_info{
      .sType{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .stage{shader_stage_create_info},
      .layout{resources.pipeline_layout},
      .basePipelineHandle{VK_NULL_HANDLE},
      .basePipelineIndex{}};

  const VkResult result{vkCreateComputePipelines(
      resources.device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr,
      &resources.pipeline)};

  vkDestroyShaderModule(resources.device, *shader_module, nullptr);

  if (result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}

[[nodiscard]] std::expected<void, std::error_code>
dispatch_compute(const vulkan_context &context,
                 const compute_pipeline_resources &resources) {
  const single_time_command_context commands{
      .device{context.device()},
      .command_pool{context.compute_command_pool()},
      .queue{context.compute_queue()}};

  const auto command_buffer{begin_single_time_commands(commands)};
  if (!command_buffer) {
    return std::unexpected(command_buffer.error());
  }

  vkCmdBindPipeline(*command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    resources.pipeline);

  vkCmdBindDescriptorSets(*command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          resources.pipeline_layout, 0, 1,
                          &resources.descriptor_set, 0, nullptr);

  constexpr auto group_count{[](const std::uint32_t extent) {
    return (extent + workgroup_size - 1u) / workgroup_size;
  }};

  vkCmdDispatch(*command_buffer, group_count(render_width),
                group_count(render_height), 1);

  return end_single_time_commands(commands, *command_buffer);
}

[[nodiscard]] std::expected<void, std::error_code>
write_image(const vulkan_buffer &storage_buffer,
            const std::filesystem::path &output_path) {
  struct pixel final {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
  };

  std::vector<std::byte> pixel_data(compute_buffer_size);
  if (const auto result{storage_buffer.read(pixel_data)}; !result) {
    return result;
  }

  const auto *const pixels{reinterpret_cast<const pixel *>(pixel_data.data())};

  std::vector<std::uint8_t> image{};
  image.reserve(rendered_image_size);
  for (std::uint32_t i{0u}; i < rendered_image_size; i += 4) {
    const float pixel_r{*reinterpret_cast<const float *>(&pixels[i + 0])};
    const float pixel_g{*reinterpret_cast<const float *>(&pixels[i + 1])};
    const float pixel_b{*reinterpret_cast<const float *>(&pixels[i + 2])};
    const float pixel_a{*reinterpret_cast<const float *>(&pixels[i + 3])};

    image.push_back(static_cast<std::uint8_t>(pixel_r * 255.0f));
    image.push_back(static_cast<std::uint8_t>(pixel_g * 255.0f));
    image.push_back(static_cast<std::uint8_t>(pixel_b * 255.0f));
    image.push_back(static_cast<std::uint8_t>(pixel_a * 255.0f));
  }

  if (stbi_write_png(output_path.string().c_str(), render_width, render_height,
                     4, image.data(), render_width * 4) == 0) {
    std::println("encoder error: Failed to write PNG");
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }

  std::println("Sucessfully rendered image");
  return {};
}

} // namespace

std::expected<void, std::error_code>
run_compute_mandelbrot_application(const std::filesystem::path &output_path) {
  auto context{vulkan_context::create({})};
  if (!context) {
    std::println("Failed to create vulkan context: {}",
                 context.error().message());
    return std::unexpected(context.error());
  }

  auto storage_buffer{vulkan_buffer::create(
      {.size{compute_buffer_size},
       .device{context->device()},
       .physical_device{context->physical_device()},
       .usage{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
       .memory_flags{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT}})};
  if (!storage_buffer) {
    return std::unexpected(storage_buffer.error());
  }

  compute_pipeline_resources resources{context->device()};
  if (const auto result{create_compute_pipeline(resources, *storage_buffer)};
      !result) {
    return result;
  }

  if (const auto result{dispatch_compute(*context, resources)}; !result) {
    return result;
  }

  return write_image(*storage_buffer, output_path);
}
