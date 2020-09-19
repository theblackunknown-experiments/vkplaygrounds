#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <vector>

#include "./vkdebug.hpp"
#include "./vkmemory.hpp"

namespace blk
{
    struct QueueFamily;

    struct PhysicalDevice
    {
        VkPhysicalDevice                     mPhysicalDevice;
        VkPhysicalDeviceFeatures             mFeatures;
        VkPhysicalDeviceProperties           mProperties;
        VkPhysicalDeviceMemoryProperties     mMemoryProperties;
        std::vector<blk::QueueFamily>        mQueueFamilies;
        std::vector<VkExtensionProperties>   mExtensions;
        blk::PhysicalDeviceMemories          mMemories;

        explicit PhysicalDevice(VkPhysicalDevice vkphysicaldevice);

        operator VkPhysicalDevice() const
        {
            return mPhysicalDevice;
        }
    };
}
