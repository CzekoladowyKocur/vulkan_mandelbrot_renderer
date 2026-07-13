#pragma once
#include "include/VulkanTypes.hpp"
#include "include/texture_2d.hpp"
#include "include/vulkan_buffer.hpp"
#include "include/vulkan_context.hpp"
#include "include/window.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

class realtime_mandelbrot_application {
public:
  bool initialize();
  bool run();
  bool shutdown();

  void on_event(const event &polled_event);

private:
  /* Vulkan Context Initialization */
  bool create_surface();
  bool create_swapchain();
  bool load_assets();

  bool create_graphics_based_pipeline();
  bool allocate_graphics_command_buffers();
  bool record_graphics_command_buffers();

  void update_frame_data(const float delta_time);
  void draw_frame();
  /* Swapchain */
  void recreate_swapchain(const uint32_t width, const uint32_t height);
  void cleanup_swapchain();

private:
  struct uniform_buffer_object {
    float aspect_ratio;
    float center_x;
    float center_y;
    float zoom_scale;
    int32_t iteration_count;

    float padding_x;
    float padding_y;
    float padding_z;
  };

private:
  bool m_running{true};
  std::optional<window> m_window;
  input m_input;
  /* Vulkan API */
  vulkan_context m_context;

  /* Surface*/
  VkSurfaceKHR m_surface{VK_NULL_HANDLE};

  /* Swapchain */
  VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};

  VkSurfaceCapabilitiesKHR m_surface_capabilities{
      .currentTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR}};
  VkSurfaceFormatKHR m_surface_format{};
  VkPresentModeKHR m_present_mode{};
  VkExtent2D m_swapchain_extent{.width{1280}, .height{720}};

  /* Swapchain sync */
  struct {
    std::vector<VkSemaphore> present_complete;
    std::vector<VkSemaphore> render_complete;
  } m_semaphores;

  uint32_t m_max_frames_in_flight{2};
  uint32_t m_image_count{};

  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;
  std::vector<VkFramebuffer> m_swapchain_framebuffers;

  /* Swapchain Compatible Renderpass */
  VkRenderPass m_swapchain_render_pass{VK_NULL_HANDLE};

  /* Pipelines*/
  /* Graphics Pipeline */
  std::optional<vulkan_buffer> m_vertex_buffer;
  std::optional<vulkan_buffer> m_index_buffer;
  std::optional<vulkan_buffer> m_ubo_buffer;

  VkShaderModule m_vertex_shader_module{VK_NULL_HANDLE};
  VkShaderModule m_fragment_shader_module{VK_NULL_HANDLE};

  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_graphics_pipeline_layout{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_graphics_pipeline_ubo_buffer_descriptor_set_layout{
      VK_NULL_HANDLE};
  VkDescriptorSetLayout m_graphics_pipeline_color_palette_descriptor_set_layout{
      VK_NULL_HANDLE};
  VkDescriptorPool m_graphics_pipeline_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_graphics_pipeline_ubo_buffer_descriptor_set{VK_NULL_HANDLE};
  VkDescriptorSet m_graphics_pipeline_color_palette_descriptor_set{
      VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> m_graphics_pipeline_command_buffers;

  /* Swapchain synchronization */
  uint32_t m_image_index{};
  uint32_t m_frame_index{};

  std::vector<VkFence> m_in_flight_fences;
  std::vector<VkFence> m_images_in_flight;

  /* Assets */
  std::optional<texture_2d> m_color_palette_texture;
};
