#pragma once
#include <filesystem>
#include <string_view>
#include "include/Core.h"
#include "include/VulkanTypes.h"

struct ImageProperties
{
	uint32_t Width, Height;
	uint32_t ChannelCount;

	ImageProperties() = default;
	ImageProperties(const uint32_t width, const uint32_t height, const uint32_t channelCount)
		:
		Width(width),
		Height(height),
		ChannelCount(channelCount)
	{}
};

class Image2D
{
public:
	explicit Image2D(const std::string_view assetPath);
	~Image2D();

	VkImage GetImageHandle();
	VkImageView GetImageView();
	VkSampler GetImageSampler();
private:
	bool Load();
private:
	std::filesystem::path m_AssetPath;
	ImageProperties m_Properties;
	Buffer m_CPUData;

	VkImage m_ImageHandle;
	VkDeviceMemory m_ImageMemory;
	VkDeviceSize m_ImageMemorySpace;

	VkImageView m_ImageView;
	VkSampler m_Sampler;
};
