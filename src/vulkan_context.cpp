#include "include/vulkan_context.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <memory>
#include <print>
#include <utility>
#include <vector>

namespace {
#ifdef APP_DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_report_callback(
    const VkDebugReportFlagsEXT flags,
    const VkDebugReportObjectTypeEXT object_type, const std::uint64_t object,
    const std::size_t location, const std::int32_t message_code,
    const char *const layer_prefix, const char *const message,
    void *const user_data) {
  (void)object;
  (void)location;
  (void)message_code;
  (void)layer_prefix;
  (void)user_data;

  if (flags) {
    std::println("VulkanDebugCallback: Object Type: {} Message: {}",
                 static_cast<int>(object_type), message);
  }

  return VK_FALSE;
}
#endif
} // namespace

std::expected<vulkan_context, std::error_code>
vulkan_context::create(vulkan_context_props &&props) noexcept {
  try {
    vulkan_context context{};
    if (const auto result{context.initialize(props)}; !result) {
      return std::unexpected(result.error());
    }

    return context;
  } catch (...) {
    return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
  }
}

std::expected<void, std::error_code>
vulkan_context::initialize(const vulkan_context_props &props) {
  if (const auto result{create_instance(props.instance_extensions)}; !result) {
    return result;
  }

  if (const auto result{select_physical_device()}; !result) {
    return result;
  }

  const bool presentation_required{std::ranges::any_of(
      props.instance_extensions, [](const char *const extension) {
        return std::strcmp(extension, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
      })};

  if (const auto result{create_device(presentation_required)}; !result) {
    return result;
  }

  return create_command_pools();
}

std::expected<void, std::error_code> vulkan_context::create_instance(
    const std::span<const char *const> instance_extensions) {
  std::vector<const char *> required_extensions(instance_extensions.begin(),
                                                instance_extensions.end());
#ifdef APP_DEBUG
  required_extensions.insert(
      required_extensions.end(),
      {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
       VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME});
  const std::vector<const char *> requested_layers{
      "VK_LAYER_KHRONOS_validation"};
#else
  const std::vector<const char *> requested_layers{};
#endif

#ifdef __APPLE__
  required_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#ifndef APP_DEBUG
  required_extensions.push_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
  constexpr VkInstanceCreateFlags instance_flags{
      VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR};
#else
  constexpr VkInstanceCreateFlags instance_flags{};
#endif

  std::uint32_t available_extension_count{};
  vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count,
                                         nullptr);
  std::vector<VkExtensionProperties> available_extensions(
      available_extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count,
                                         available_extensions.data());

  for (const char *const required_extension : required_extensions) {
    bool found{false};
    for (const VkExtensionProperties &available : available_extensions) {
      if (std::strcmp(required_extension, available.extensionName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      std::println("Failed to find instance extension with name: {}",
                   required_extension);
      return std::unexpected(
          std::make_error_code(std::errc::function_not_supported));
    }
  }

  std::uint32_t available_layer_count{};
  vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);
  std::vector<VkLayerProperties> available_layers(available_layer_count);
  vkEnumerateInstanceLayerProperties(&available_layer_count,
                                     available_layers.data());

  std::vector<const char *> available_requested_layers{};
  for (const char *const requested_layer : requested_layers) {
    bool found{false};
    for (const VkLayerProperties &available : available_layers) {
      if (std::strcmp(requested_layer, available.layerName) == 0) {
        found = true;
        break;
      }
    }

    if (found) {
      available_requested_layers.push_back(requested_layer);
    } else {
      std::println(
          "Failed to find a requested instance-level layer with given name: "
          "{}",
          requested_layer);
    }
  }

  const VkApplicationInfo application_info{
      .sType{VK_STRUCTURE_TYPE_APPLICATION_INFO},
      .pNext{nullptr},
      .pApplicationName{"Mandelbrot Renderer"},
      .applicationVersion{VK_MAKE_VERSION(0, 0, 1)},
      .pEngineName{"Good engine name"},
      .engineVersion{VK_MAKE_VERSION(0, 0, 1)},
      .apiVersion{VK_API_VERSION_1_2}};

  const VkInstanceCreateInfo instance_create_info{
      .sType{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO},
      .pNext{nullptr},
      .flags{instance_flags},
      .pApplicationInfo{&application_info},
      .enabledLayerCount{
          static_cast<std::uint32_t>(available_requested_layers.size())},
      .ppEnabledLayerNames{available_requested_layers.data()},
      .enabledExtensionCount{
          static_cast<std::uint32_t>(required_extensions.size())},
      .ppEnabledExtensionNames{required_extensions.data()}};

  if (const VkResult result{
          vkCreateInstance(&instance_create_info, nullptr, &m_instance)};
      result != VK_SUCCESS) {
    std::println("Failed to initialize vulkan instance. You may want to "
                 "update your drivers");
    return make_vulkan_error(result);
  }

#ifdef APP_DEBUG
  const auto create_debug_report_callback{
      reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
          vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT"))};
  if (create_debug_report_callback != nullptr) {
    const VkDebugReportCallbackCreateInfoEXT debug_report_create_info{
        .sType{VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT},
        .pNext{nullptr},
        .flags{VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
               VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT},
        .pfnCallback{reinterpret_cast<PFN_vkDebugReportCallbackEXT>(
            vulkan_debug_report_callback)},
        .pUserData{nullptr}};

    create_debug_report_callback(m_instance, &debug_report_create_info, nullptr,
                                 &m_debug_report_callback);
  }
#endif

  return {};
}

std::expected<void, std::error_code> vulkan_context::select_physical_device() {
  std::uint32_t physical_device_count{};
  if (const VkResult result{vkEnumeratePhysicalDevices(
          m_instance, &physical_device_count, nullptr)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (physical_device_count == 0) {
    return std::unexpected(std::make_error_code(std::errc::no_such_device));
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
  if (const VkResult result{vkEnumeratePhysicalDevices(
          m_instance, &physical_device_count, physical_devices.data())};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  m_physical_device = physical_devices[0];
  for (const VkPhysicalDevice physical_device : physical_devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      m_physical_device = physical_device;
      break;
    }
  }

  std::uint32_t queue_family_count{};
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device,
                                           &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      m_physical_device, &queue_family_count, queue_families.data());

  bool graphics_found{false};
  bool compute_found{false};

  for (std::uint32_t i{0u}; i < queue_family_count; ++i) {
    const VkQueueFlags queue_flags{queue_families[i].queueFlags};

    if (!graphics_found && (queue_flags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      m_graphics_queue_family = i;
      graphics_found = true;
    }

    if ((queue_flags & VK_QUEUE_COMPUTE_BIT) != 0 &&
        (queue_flags & VK_QUEUE_GRAPHICS_BIT) == 0) {
      m_compute_queue_family = i;
      compute_found = true;
    }
  }

  if (!compute_found) {
    for (std::uint32_t i{0u}; i < queue_family_count; ++i) {
      if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
        m_compute_queue_family = i;
        compute_found = true;
        break;
      }
    }
  }

  if (!graphics_found || !compute_found) {
    return std::unexpected(std::make_error_code(std::errc::no_such_device));
  }

  return {};
}

std::expected<void, std::error_code>
vulkan_context::create_device(const bool presentation_required) {
  std::vector<const char *> required_device_extensions{};
  if (presentation_required) {
    required_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  std::uint32_t available_extension_count{};
  vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr,
                                       &available_extension_count, nullptr);
  std::vector<VkExtensionProperties> available_extensions(
      available_extension_count);
  vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr,
                                       &available_extension_count,
                                       available_extensions.data());

  constexpr const char *portability_subset_extension{
      "VK_KHR_portability_subset"};
  for (const VkExtensionProperties &available : available_extensions) {
    if (std::strcmp(portability_subset_extension, available.extensionName) ==
        0) {
      required_device_extensions.push_back(portability_subset_extension);
      break;
    }
  }

  constexpr float default_queue_priority{0.0f};

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos{
      {.sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
       .pNext{nullptr},
       .flags{},
       .queueFamilyIndex{m_graphics_queue_family},
       .queueCount{1u},
       .pQueuePriorities{&default_queue_priority}}};

  if (m_compute_queue_family != m_graphics_queue_family) {
    queue_create_infos.push_back(
        {.sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
         .pNext{nullptr},
         .flags{},
         .queueFamilyIndex{m_compute_queue_family},
         .queueCount{1u},
         .pQueuePriorities{&default_queue_priority}});
  }

  VkPhysicalDeviceFeatures supported_features{};
  vkGetPhysicalDeviceFeatures(m_physical_device, &supported_features);

  const VkPhysicalDeviceFeatures enabled_features{
      .shaderFloat64{supported_features.shaderFloat64}};

  const VkDeviceCreateInfo device_create_info{
      .sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .queueCreateInfoCount{
          static_cast<std::uint32_t>(queue_create_infos.size())},
      .pQueueCreateInfos{queue_create_infos.data()},
      .enabledLayerCount{},
      .ppEnabledLayerNames{nullptr},
      .enabledExtensionCount{
          static_cast<std::uint32_t>(required_device_extensions.size())},
      .ppEnabledExtensionNames{required_device_extensions.data()},
      .pEnabledFeatures{&enabled_features}};

  if (const VkResult result{vkCreateDevice(
          m_physical_device, &device_create_info, nullptr, &m_device)};
      result != VK_SUCCESS) {
    std::println("Failed to create vulkan logical device");
    return make_vulkan_error(result);
  }

  vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_compute_queue_family, 0, &m_compute_queue);

  return {};
}

std::expected<void, std::error_code> vulkan_context::create_command_pools() {
  const VkCommandPoolCreateInfo graphics_pool_create_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
      .pNext{nullptr},
      .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
      .queueFamilyIndex{m_graphics_queue_family}};

  if (const VkResult result{
          vkCreateCommandPool(m_device, &graphics_pool_create_info, nullptr,
                              &m_graphics_command_pool)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  const VkCommandPoolCreateInfo compute_pool_create_info{
      .sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
      .pNext{nullptr},
      .flags{VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
      .queueFamilyIndex{m_compute_queue_family}};

  if (const VkResult result{
          vkCreateCommandPool(m_device, &compute_pool_create_info, nullptr,
                              &m_compute_command_pool)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}

vulkan_context::vulkan_context(vulkan_context &&other) noexcept
    : m_instance{other.m_instance},
#ifdef APP_DEBUG
      m_debug_report_callback{other.m_debug_report_callback},
#endif
      m_physical_device{other.m_physical_device},
      m_graphics_queue_family{other.m_graphics_queue_family},
      m_compute_queue_family{other.m_compute_queue_family},
      m_device{other.m_device}, m_graphics_queue{other.m_graphics_queue},
      m_compute_queue{other.m_compute_queue},
      m_graphics_command_pool{other.m_graphics_command_pool},
      m_compute_command_pool{other.m_compute_command_pool} {
  other.m_instance = VK_NULL_HANDLE;
#ifdef APP_DEBUG
  other.m_debug_report_callback = VK_NULL_HANDLE;
#endif
  other.m_physical_device = VK_NULL_HANDLE;
  other.m_graphics_queue_family = {};
  other.m_compute_queue_family = {};
  other.m_device = VK_NULL_HANDLE;
  other.m_graphics_queue = VK_NULL_HANDLE;
  other.m_compute_queue = VK_NULL_HANDLE;
  other.m_graphics_command_pool = VK_NULL_HANDLE;
  other.m_compute_command_pool = VK_NULL_HANDLE;
}

vulkan_context &vulkan_context::operator=(vulkan_context &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  m_instance = other.m_instance;
#ifdef APP_DEBUG
  m_debug_report_callback = other.m_debug_report_callback;
#endif
  m_physical_device = other.m_physical_device;
  m_graphics_queue_family = other.m_graphics_queue_family;
  m_compute_queue_family = other.m_compute_queue_family;
  m_device = other.m_device;
  m_graphics_queue = other.m_graphics_queue;
  m_compute_queue = other.m_compute_queue;
  m_graphics_command_pool = other.m_graphics_command_pool;
  m_compute_command_pool = other.m_compute_command_pool;

  other.m_instance = VK_NULL_HANDLE;
#ifdef APP_DEBUG
  other.m_debug_report_callback = VK_NULL_HANDLE;
#endif
  other.m_physical_device = VK_NULL_HANDLE;
  other.m_graphics_queue_family = {};
  other.m_compute_queue_family = {};
  other.m_device = VK_NULL_HANDLE;
  other.m_graphics_queue = VK_NULL_HANDLE;
  other.m_compute_queue = VK_NULL_HANDLE;
  other.m_graphics_command_pool = VK_NULL_HANDLE;
  other.m_compute_command_pool = VK_NULL_HANDLE;

  return *this;
}

vulkan_context::~vulkan_context() { destroy(); }

void vulkan_context::destroy() noexcept {
  if (m_instance == VK_NULL_HANDLE) {
    return;
  }

  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
    vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);
    vkDestroyCommandPool(m_device, m_compute_command_pool, nullptr);
    vkDestroyDevice(m_device, nullptr);
  }

#ifdef APP_DEBUG
  if (m_debug_report_callback != VK_NULL_HANDLE) {
    const auto destroy_debug_report_callback{
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(m_instance,
                                  "vkDestroyDebugReportCallbackEXT"))};
    if (destroy_debug_report_callback != nullptr) {
      destroy_debug_report_callback(m_instance, m_debug_report_callback,
                                    nullptr);
    }
  }
  m_debug_report_callback = VK_NULL_HANDLE;
#endif

  vkDestroyInstance(m_instance, nullptr);

  m_instance = VK_NULL_HANDLE;
  m_physical_device = VK_NULL_HANDLE;
  m_graphics_queue_family = {};
  m_compute_queue_family = {};
  m_device = VK_NULL_HANDLE;
  m_graphics_queue = VK_NULL_HANDLE;
  m_compute_queue = VK_NULL_HANDLE;
  m_graphics_command_pool = VK_NULL_HANDLE;
  m_compute_command_pool = VK_NULL_HANDLE;
}

VkInstance vulkan_context::instance() const noexcept { return m_instance; }

VkPhysicalDevice vulkan_context::physical_device() const noexcept {
  return m_physical_device;
}

VkDevice vulkan_context::device() const noexcept { return m_device; }

VkQueue vulkan_context::graphics_queue() const noexcept {
  return m_graphics_queue;
}

VkQueue vulkan_context::compute_queue() const noexcept {
  return m_compute_queue;
}

std::uint32_t vulkan_context::graphics_queue_family() const noexcept {
  return m_graphics_queue_family;
}

std::uint32_t vulkan_context::compute_queue_family() const noexcept {
  return m_compute_queue_family;
}

VkCommandPool vulkan_context::graphics_command_pool() const noexcept {
  return m_graphics_command_pool;
}

VkCommandPool vulkan_context::compute_command_pool() const noexcept {
  return m_compute_command_pool;
}

std::expected<VkShaderModule, std::error_code>
create_shader_module(const VkDevice device,
                     const std::filesystem::path &path) noexcept {
  try {
    std::ifstream file{path, std::ios::ate | std::ios::binary};
    if (!file.is_open()) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const std::streampos file_size{file.tellg()};
    std::vector<char> code(static_cast<std::size_t>(file_size));
    file.seekg(std::ios::beg);
    file.read(code.data(), static_cast<std::streamsize>(file_size));
    if (!file) {
      return std::unexpected(std::make_error_code(std::errc::io_error));
    }

    const VkShaderModuleCreateInfo shader_module_create_info{
        .sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO},
        .pNext{nullptr},
        .flags{},
        .codeSize{code.size()},
        .pCode{reinterpret_cast<const std::uint32_t *>(code.data())}};

    VkShaderModule shader_module{VK_NULL_HANDLE};
    if (const VkResult result{vkCreateShaderModule(
            device, &shader_module_create_info, nullptr, &shader_module)};
        result != VK_SUCCESS) {
      return make_vulkan_error(result);
    }

    return shader_module;
  } catch (...) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}
