#pragma once
#include "include/Core.hpp"
#include "include/Image2D.hpp"
#include "include/VulkanTypes.hpp"
#include "include/window.hpp"
#include <optional>
#include <string_view>

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

public:
  static bool Close();
  static VulkanApp *GetInstance();

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
  uint32_t RetrieveMemoryTypeIndex(VkMemoryPropertyFlags memoryPropertyFlags,
                                   uint32_t memoryTypeBits) const;

  VkCommandBuffer BeginRecordingSingleTimeUseCommands(const bool compute);
  void EndRecordingSingleTimeUseCommands(VkCommandBuffer commandBuffer,
                                         const bool compute);

  void InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage image,
                                VkAccessFlags srcAccessMask,
                                VkAccessFlags dstAccessMask,
                                VkImageLayout oldImageLayout,
                                VkImageLayout newImageLayout,
                                VkPipelineStageFlags srcStageMask,
                                VkPipelineStageFlags dstStageMask,
                                VkImageSubresourceRange subresourceRange);

  void SetImageLayout(VkCommandBuffer cmdbuffer, VkImage image,
                      VkImageLayout oldImageLayout,
                      VkImageLayout newImageLayout,
                      VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask);

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
  ERenderMethod m_RenderMethod;
  bool m_Running;
  std::optional<window> m_window;
  input m_input;
  /* Vulkan API */
  /* Instance (loads the vulkan dll driver) */
  VkInstance m_Instance;

  /* Surface*/
  VkSurfaceKHR m_Surface;

  /* Physical Device */
  VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
  VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures;
  mutable VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;
  VkPhysicalDevice m_PhysicalDevice;

  QueueFamilyIndices m_QueueIndices;

  /* Logical Device */
  VkDevice m_LogicalDevice;
  VkQueue m_GraphicsQueue;
  VkQueue m_ComputeQueue;
  VkQueue m_PresentQueue;
  VkCommandPool m_GraphicsCommandPool;
  VkCommandPool m_ComputeCommandPool;

  /* Swapchain */
  VkSwapchainKHR m_Swapchain;

  VkSurfaceCapabilitiesKHR m_SurfaceCapabilities;
  VkSurfaceFormatKHR m_SurfaceFormat;
  VkPresentModeKHR m_PresentMode;
  VkExtent2D m_SwapchainExtent;

  /* Swapchain sync */
  struct {
    std::vector<VkSemaphore> PresentComplete;
    std::vector<VkSemaphore> RenderComplete;
  } m_Semaphores;

  uint32_t m_MaxFramesInFlight;
  uint32_t m_ImageCount;
  uint32_t m_MinimalImageCount;

  std::vector<VkImage> m_SwapchainImages;
  std::vector<VkImageView> m_SwapchainImageViews;
  std::vector<VkFramebuffer> m_SwapchainFramebuffers;

  /* Swapchain Compatible Renderpass */
  VkRenderPass m_SwapchainRenderPass;

  /* Pipelines*/
  /* Graphics Pipeline */
  VulkanBuffer m_VertexBuffer;
  VulkanBuffer m_IndexBuffer;
  VulkanBuffer m_UBOBuffer;

  VkShaderModule m_VertexShaderModule;
  VkShaderModule m_FragmentShaderModule;

  VkPipeline m_GraphicsPipeline;
  VkPipelineLayout m_GraphicsPipelineLayout;

  VkDescriptorSetLayout m_GraphicsPipelineUBOBufferDescriptorSetLayout;
  VkDescriptorSetLayout m_GraphicsPipelineColorPaletteDescriptorSetLayout;
  VkDescriptorPool m_GraphicsPipelineDescriptorPool;
  VkDescriptorSet m_GraphicsPipelineUBOBufferDescriptorSet;
  VkDescriptorSet m_GraphicsPipelineColorPaletteDescriptorSet;
  std::vector<VkCommandBuffer> m_GraphicsPipelineCommandBuffers;

  /* Compute Pipeline */
  VulkanBuffer m_ComputePipelineStorageBuffer;

  VkShaderModule m_ComputeShaderModule;
  VkDescriptorSetLayout m_ComputePipelineDescriptorSetLayout;
  VkDescriptorPool m_ComputePipelineDescriptorPool;
  VkDescriptorSet m_ComputePipelineStorageBufferDescriptorSet;

  VkPipeline m_ComputePipeline;
  VkPipelineLayout m_ComputePipelineLayout;

  VkCommandBuffer m_ComputePipelineCommandBuffer;
  /* Swapchain synchronization */
  uint32_t m_ImageIndex;
  uint32_t m_FrameIndex;

  std::vector<VkFence> m_InFlightFences;
  std::vector<VkFence> m_ImagesInFlight;

  /* Assets */
  Image2D *m_ColorPaletteImage;

  /* Debug */
#ifdef APP_DEBUG
  VkDebugReportCallbackEXT m_DebugReportCallback;
#endif
private:
  static VulkanApp *s_ApplicationInstance;

private:
  friend class Image2D;
};
