#pragma once
#include "include/VulkanTypes.hpp"
#include "include/texture_2d.hpp"
#include "include/vulkan_buffer.hpp"
#include "include/window.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

class VulkanApp {
public:
  enum class ERenderMethod : std::uint8_t {
    Graphics,
    Compute,
    Default = Graphics,
  };

public:
  explicit VulkanApp(const ERenderMethod renderMethod);
  ~VulkanApp();

  bool Initialize();
  bool Run();
  bool Shutdown();

  void OnEvent(const event &polled_event);

private:
  /* Vulkan Context Initialization */
  bool CreateInstance();
  bool CreateSurface();
  bool CreateLogicalDevice();
  bool CreateSwapchain();
  bool LoadAssets();

  bool CreateGraphicsBasedPipeline();
  bool CreateComputeBasedPipeline();
  bool AllocateGraphicsCommandBuffers();
  bool AllocateComputeCommandBuffers();
  bool RecordGraphicsCommandBuffers();
  bool RecordComputeCommandBuffers();

  void UpdateFrameData(const float deltaTime);
  void DrawFrame();
  /* Swapchain */
  void RecreateSwapchain(const uint32_t width, const uint32_t height);
  void CleanupSwapchain();
  /* Pipeline */
  VkShaderModule CreateShaderModule(const std::string_view filepath) const;

private:
  struct UBO {
    float AspectRatio;
    float CenterX;
    float CenterY;
    float ZoomScale;
    int32_t IterationCount;

    float padding_x;
    float padding_y;
    float padding_z;
  };

  struct QueueFamilyIndices {
    int32_t Graphics = -1;
    int32_t Compute = -1;
    int32_t Transfer = -1;
  };

  QueueFamilyIndices GetQueueFamilyIndices(int32_t flags);

private:
  ERenderMethod m_RenderMethod{ERenderMethod::Default};
  bool m_Running{true};
  std::optional<window> m_window;
  input m_input;
  /* Vulkan API */
  /* Instance (loads the vulkan dll driver) */
  VkInstance m_Instance{VK_NULL_HANDLE};

  /* Surface*/
  VkSurfaceKHR m_Surface{VK_NULL_HANDLE};

  /* Physical Device */
  VkPhysicalDeviceProperties m_PhysicalDeviceProperties{};
  VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures{};
  mutable VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties{};
  VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};

  QueueFamilyIndices m_QueueIndices{};

  /* Logical Device */
  VkDevice m_LogicalDevice{VK_NULL_HANDLE};
  VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
  VkQueue m_ComputeQueue{VK_NULL_HANDLE};
  VkQueue m_PresentQueue{VK_NULL_HANDLE};
  VkCommandPool m_GraphicsCommandPool{VK_NULL_HANDLE};
  VkCommandPool m_ComputeCommandPool{VK_NULL_HANDLE};

  /* Swapchain */
  VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};

  VkSurfaceCapabilitiesKHR m_SurfaceCapabilities{
      .currentTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR}};
  VkSurfaceFormatKHR m_SurfaceFormat{};
  VkPresentModeKHR m_PresentMode{};
  VkExtent2D m_SwapchainExtent{.width{1280}, .height{720}};

  /* Swapchain sync */
  struct {
    std::vector<VkSemaphore> PresentComplete;
    std::vector<VkSemaphore> RenderComplete;
  } m_Semaphores;

  uint32_t m_MaxFramesInFlight{2};
  uint32_t m_ImageCount{};

  std::vector<VkImage> m_SwapchainImages;
  std::vector<VkImageView> m_SwapchainImageViews;
  std::vector<VkFramebuffer> m_SwapchainFramebuffers;

  /* Swapchain Compatible Renderpass */
  VkRenderPass m_SwapchainRenderPass{VK_NULL_HANDLE};

  /* Pipelines*/
  /* Graphics Pipeline */
  std::optional<vulkan_buffer> m_VertexBuffer;
  std::optional<vulkan_buffer> m_IndexBuffer;
  std::optional<vulkan_buffer> m_UBOBuffer;

  VkShaderModule m_VertexShaderModule{VK_NULL_HANDLE};
  VkShaderModule m_FragmentShaderModule{VK_NULL_HANDLE};

  VkPipeline m_GraphicsPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_GraphicsPipelineLayout{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_GraphicsPipelineUBOBufferDescriptorSetLayout{
      VK_NULL_HANDLE};
  VkDescriptorSetLayout m_GraphicsPipelineColorPaletteDescriptorSetLayout{
      VK_NULL_HANDLE};
  VkDescriptorPool m_GraphicsPipelineDescriptorPool{VK_NULL_HANDLE};
  VkDescriptorSet m_GraphicsPipelineUBOBufferDescriptorSet{VK_NULL_HANDLE};
  VkDescriptorSet m_GraphicsPipelineColorPaletteDescriptorSet{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> m_GraphicsPipelineCommandBuffers;

  /* Compute Pipeline */
  std::optional<vulkan_buffer> m_ComputePipelineStorageBuffer;

  VkShaderModule m_ComputeShaderModule{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_ComputePipelineDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorPool m_ComputePipelineDescriptorPool{VK_NULL_HANDLE};
  VkDescriptorSet m_ComputePipelineStorageBufferDescriptorSet{VK_NULL_HANDLE};

  VkPipeline m_ComputePipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_ComputePipelineLayout{VK_NULL_HANDLE};

  VkCommandBuffer m_ComputePipelineCommandBuffer{VK_NULL_HANDLE};
  /* Swapchain synchronization */
  uint32_t m_ImageIndex{};
  uint32_t m_FrameIndex{};

  std::vector<VkFence> m_InFlightFences;
  std::vector<VkFence> m_ImagesInFlight;

  /* Assets */
  std::optional<texture_2d> m_ColorPaletteTexture;

  /* Debug */
#ifdef APP_DEBUG
  VkDebugReportCallbackEXT m_DebugReportCallback{VK_NULL_HANDLE};
#endif
};
