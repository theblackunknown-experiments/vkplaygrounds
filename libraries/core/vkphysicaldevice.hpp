#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <vector>

#include "./vkdebug.hpp"
#include "./vkqueue.hpp"
#include "./vkmemory.hpp"

#include <core_export.h>

namespace blk
{
struct QueueFamily;

struct PhysicalDevice
{
	VkPhysicalDevice mPhysicalDevice;
	VkPhysicalDeviceFeatures mFeatures;
	VkPhysicalDeviceProperties mProperties;
	VkPhysicalDeviceMemoryProperties mMemoryProperties;
	std::vector<blk::QueueFamily> mQueueFamilies;
	std::vector<VkExtensionProperties> mExtensions;
	blk::PhysicalDeviceMemories mMemories;

	CORE_EXPORT explicit PhysicalDevice(VkPhysicalDevice vkphysicaldevice);

	operator VkPhysicalDevice() const { return mPhysicalDevice; }
};

[[nodiscard]] CORE_EXPORT std::vector<blk::PhysicalDevice> physicaldevices(VkInstance instance);
} // namespace blk
