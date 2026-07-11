#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#ifdef APP_DEBUG
#define VK_CHECK(x) if(x != VK_SUCCESS) \
assert(false)
#else
#define VK_CHECK(x) x
#endif

struct VulkanBuffer
{
	VkBuffer Handle = VK_NULL_HANDLE;
	VkDeviceMemory DeviceMemory = VK_NULL_HANDLE;
	Buffer CPUData;
};