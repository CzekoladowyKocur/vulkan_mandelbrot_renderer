#pragma once
#include "include/VulkanTypes.hpp"
#include "include/input.hpp"
#include "include/vulkan_buffer.hpp"
#include "include/vulkan_context.hpp"
#include "include/window.hpp"
#include <cstdint>
#include <expected>
#include <system_error>
#include <vector>

class realtime_mandelbrot_application {
public:
  [[nodiscard]] std::expected<void, std::error_code> run();

private:
  struct uniform_buffer_object {
    float aspect_ratio{};
    float center_x{};
    float center_y{};
    float zoom_scale{};
    std::int32_t iteration_count{};

    float padding_x{};
    float padding_y{};
    float padding_z{};
  };

  struct surface_resource final {
    VkInstance instance{VK_NULL_HANDLE};
    VkSurfaceKHR handle{VK_NULL_HANDLE};

    surface_resource() = default;
    surface_resource(const surface_resource &) = delete;
    surface_resource &operator=(const surface_resource &) = delete;
    surface_resource(surface_resource &&other) noexcept;
    surface_resource &operator=(surface_resource &&other) noexcept;
    ~surface_resource();
  };

  struct swapchain_resources final {
    VkDevice device{VK_NULL_HANDLE};
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkSurfaceFormatKHR surface_format{};
    VkExtent2D extent{};
    std::uint32_t image_count{};
    std::uint32_t max_frames_in_flight{};
    VkRenderPass render_pass{VK_NULL_HANDLE};
    std::vector<VkImage> images{};
    std::vector<VkImageView> image_views{};
    std::vector<VkFramebuffer> framebuffers{};
    std::vector<VkSemaphore> present_complete{};
    std::vector<VkSemaphore> render_complete{};
    std::vector<VkFence> in_flight_fences{};
    std::vector<VkFence> images_in_flight{};

    swapchain_resources() = default;
    swapchain_resources(const swapchain_resources &) = delete;
    swapchain_resources &operator=(const swapchain_resources &) = delete;
    swapchain_resources(swapchain_resources &&other) noexcept;
    swapchain_resources &operator=(swapchain_resources &&other) noexcept;
    ~swapchain_resources();

  private:
    void destroy() noexcept;
  };

  struct pipeline_resources final {
    VkDevice device{VK_NULL_HANDLE};
    VkDescriptorSetLayout ubo_descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSet ubo_descriptor_set{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    pipeline_resources() = default;
    pipeline_resources(const pipeline_resources &) = delete;
    pipeline_resources &operator=(const pipeline_resources &) = delete;
    pipeline_resources(pipeline_resources &&other) noexcept;
    pipeline_resources &operator=(pipeline_resources &&other) noexcept;
    ~pipeline_resources();

  private:
    void destroy() noexcept;
  };

  struct graphics_command_buffers final {
    VkDevice device{VK_NULL_HANDLE};
    VkCommandPool command_pool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> buffers{};

    graphics_command_buffers() = default;
    graphics_command_buffers(const graphics_command_buffers &) = delete;
    graphics_command_buffers &
    operator=(const graphics_command_buffers &) = delete;
    graphics_command_buffers(graphics_command_buffers &&other) noexcept;
    graphics_command_buffers &
    operator=(graphics_command_buffers &&other) noexcept;
    ~graphics_command_buffers();

  private:
    void destroy() noexcept;
  };

  void on_event(const event &polled_event);

  [[nodiscard]] static std::expected<surface_resource, std::error_code>
  create_surface(const vulkan_context &context, window &target_window);

  [[nodiscard]] std::expected<swapchain_resources, std::error_code>
  create_swapchain_resources(const vulkan_context &context,
                             const VkSurfaceKHR surface);

  [[nodiscard]] static std::expected<pipeline_resources, std::error_code>
  create_pipeline_resources(const vulkan_context &context,
                            const swapchain_resources &swapchain,
                            const vulkan_buffer &ubo_buffer);

  [[nodiscard]] static std::expected<graphics_command_buffers, std::error_code>
  allocate_graphics_command_buffers(const vulkan_context &context,
                                    const std::uint32_t count);

  [[nodiscard]] static std::expected<void, std::error_code>
  record_graphics_command_buffers(
      const swapchain_resources &swapchain, const pipeline_resources &pipeline,
      const vulkan_buffer &vertex_buffer, const vulkan_buffer &index_buffer,
      const graphics_command_buffers &command_buffers);

  [[nodiscard]] std::expected<void, std::error_code>
  update_frame_data(const float delta_time, const window &target_window,
                    uniform_buffer_object &ubo, float &zoom_scale,
                    vulkan_buffer &ubo_buffer);

  [[nodiscard]] static std::expected<bool, std::error_code>
  draw_frame(const vulkan_context &context, swapchain_resources &swapchain,
             const graphics_command_buffers &command_buffers,
             std::uint32_t &frame_index);

  bool m_running{true};
  input m_input{};
  VkExtent2D m_swapchain_extent{.width{1280u}, .height{720u}};
};
