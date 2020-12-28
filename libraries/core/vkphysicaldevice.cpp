#include "./vkphysicaldevice.hpp"

#include "./vkqueue.hpp"

#include <vulkan/vulkan_core.h>

namespace blk
{

PhysicalDevice::PhysicalDevice(VkPhysicalDevice vkphysicaldevice): mPhysicalDevice(vkphysicaldevice)
{
	vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
	vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
	vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
	mMemories.initialize(mMemoryProperties);
	{ // Queue Family Properties
		std::uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, nullptr);
		assert(count > 0);
		mQueueFamilies.reserve(count);
		std::vector<VkQueueFamilyProperties> v(count);
		vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, v.data());
		for (std::uint32_t idx = 0; idx < count; ++idx)
			mQueueFamilies.emplace_back(*this, idx, v.at(idx));
	}
	{ // Extensions
		std::uint32_t count;
		CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, nullptr));
		mExtensions.resize(count);
		CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, mExtensions.data()));
	}
}

std::vector<blk::PhysicalDevice> physicaldevices(VkInstance instance)
{
	std::uint32_t count = 0;
	CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
	assert(count > 0);

	std::vector<VkPhysicalDevice> vkphysicaldevices(count);
	CHECK(vkEnumeratePhysicalDevices(instance, &count, vkphysicaldevices.data()));

	std::vector<blk::PhysicalDevice> physicaldevices;
	physicaldevices.reserve(count);
	for (auto&& vkphysicaldevice : vkphysicaldevices)
		physicaldevices.emplace_back(vkphysicaldevice);
	return physicaldevices;
}

} // namespace blk
