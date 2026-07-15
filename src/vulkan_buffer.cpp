#include "vulkan_buffer.hpp"
#include <cstring>
#include <memory>

std::expected<vulkan_buffer, std::error_code>
vulkan_buffer::create(vulkan_buffer_props &&props) noexcept {
  if (props.device == VK_NULL_HANDLE ||
      props.physical_device == VK_NULL_HANDLE || props.size == 0 ||
      props.usage == 0 || props.memory_flags == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  vulkan_buffer buffer{props.size, props.device};
  if (const auto result{buffer.initialize(props.physical_device, props.usage,
                                          props.memory_flags)};
      !result) {
    return std::unexpected(result.error());
  }

  return buffer;
}

vulkan_buffer::vulkan_buffer(const VkDeviceSize size,
                             const VkDevice device) noexcept
    : m_size{size}, m_device{device} {}

std::expected<void, std::error_code>
vulkan_buffer::initialize(const VkPhysicalDevice physical_device,
                          const VkBufferUsageFlags usage,
                          const VkMemoryPropertyFlags memory_flags) {
  const VkBufferCreateInfo buffer_create_info{
      .sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
      .pNext{nullptr},
      .flags{},
      .size{m_size},
      .usage{usage},
      .sharingMode{VK_SHARING_MODE_EXCLUSIVE},
      .queueFamilyIndexCount{VK_QUEUE_FAMILY_IGNORED},
      .pQueueFamilyIndices{nullptr}};

  if (const VkResult result{
          vkCreateBuffer(m_device, &buffer_create_info, nullptr, &m_handle)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  VkMemoryRequirements memory_requirements{};
  vkGetBufferMemoryRequirements(m_device, m_handle, &memory_requirements);

  const VkMemoryAllocateInfo allocate_info{
      .sType{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO},
      .pNext{nullptr},
      .allocationSize{memory_requirements.size},
      .memoryTypeIndex{retrieve_memory_type_index(
          physical_device, memory_requirements.memoryTypeBits, memory_flags)}};

  if (const VkResult result{
          vkAllocateMemory(m_device, &allocate_info, nullptr, &m_memory)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  if (const VkResult result{
          vkBindBufferMemory(m_device, m_handle, m_memory, 0)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return {};
}

std::expected<void, std::error_code>
vulkan_buffer::write(const std::span<const std::byte> data) noexcept {
  if (data.size() > m_size) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  void *mapped{nullptr};
  if (const VkResult result{
          vkMapMemory(m_device, m_memory, 0, data.size(), 0, &mapped)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::memcpy(mapped, data.data(), data.size());
  vkUnmapMemory(m_device, m_memory);

  return {};
}

std::expected<void, std::error_code>
vulkan_buffer::read(const std::span<std::byte> destination) const noexcept {
  if (destination.size() > m_size) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  void *mapped{nullptr};
  if (const VkResult result{
          vkMapMemory(m_device, m_memory, 0, destination.size(), 0, &mapped)};
      result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  std::memcpy(destination.data(), mapped, destination.size());
  vkUnmapMemory(m_device, m_memory);

  return {};
}

vulkan_buffer::vulkan_buffer(vulkan_buffer &&other) noexcept
    : m_size{other.m_size}, m_device{other.m_device}, m_handle{other.m_handle},
      m_memory{other.m_memory} {
  other.m_size = {};
  other.m_device = VK_NULL_HANDLE;
  other.m_handle = VK_NULL_HANDLE;
  other.m_memory = VK_NULL_HANDLE;
}

vulkan_buffer &vulkan_buffer::operator=(vulkan_buffer &&other) noexcept {
  if (this == std::addressof(other)) {
    return *this;
  }

  destroy();

  m_size = other.m_size;
  m_device = other.m_device;
  m_handle = other.m_handle;
  m_memory = other.m_memory;

  other.m_size = {};
  other.m_device = VK_NULL_HANDLE;
  other.m_handle = VK_NULL_HANDLE;
  other.m_memory = VK_NULL_HANDLE;

  return *this;
}

vulkan_buffer::~vulkan_buffer() { destroy(); }

void vulkan_buffer::destroy() noexcept {
  if (m_device == VK_NULL_HANDLE) {
    return;
  }

  vkFreeMemory(m_device, m_memory, nullptr);
  vkDestroyBuffer(m_device, m_handle, nullptr);

  m_size = {};
  m_device = VK_NULL_HANDLE;
  m_memory = VK_NULL_HANDLE;
  m_handle = VK_NULL_HANDLE;
}

VkBuffer vulkan_buffer::handle() const noexcept { return m_handle; }

VkDeviceSize vulkan_buffer::size() const noexcept { return m_size; }

std::expected<void, std::error_code>
copy_buffer(const single_time_command_context &commands, const VkBuffer source,
            const VkBuffer destination, const VkDeviceSize size) noexcept {
  const auto command_buffer{begin_single_time_commands(commands)};
  if (!command_buffer) {
    return std::unexpected(command_buffer.error());
  }

  const VkBufferCopy copy_region{.srcOffset{}, .dstOffset{}, .size{size}};

  vkCmdCopyBuffer(*command_buffer, source, destination, 1, &copy_region);

  return end_single_time_commands(commands, *command_buffer);
}
