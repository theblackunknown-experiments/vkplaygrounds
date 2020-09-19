#include "./vkphysicaldevice.hpp"

#include "./vkqueue.hpp"

#include <vulkan/vulkan_core.h>

namespace blk
{

PhysicalDevice::PhysicalDevice(VkPhysicalDevice vkphysicaldevice)
    : mPhysicalDevice(vkphysicaldevice)
{
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
    mMemories.initialize(mMemoryProperties);
    {// Queue Family Properties
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, nullptr);
        assert(count > 0);
        mQueueFamilies.reserve(count);
        std::vector<VkQueueFamilyProperties> v(count);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, v.data());
        for (std::uint32_t idx = 0; idx < count; ++idx)
            mQueueFamilies.emplace_back(idx, properties);
    }
    {// Extensions
        std::uint32_t count;
        CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, nullptr));
        mExtensions.resize(count);
        CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, mExtensions.data()));
    }
}

}
