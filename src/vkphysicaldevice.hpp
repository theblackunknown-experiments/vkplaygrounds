#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <vector>

#include "./vkdebug.hpp"
#include "./vkmemory.hpp"

namespace blk
{
    struct PhysicalDevice
    {
        VkPhysicalDevice                     mPhysicalDevice;
        VkPhysicalDeviceFeatures             mFeatures;
        VkPhysicalDeviceProperties           mProperties;
        VkPhysicalDeviceMemoryProperties     mMemoryProperties;
        std::vector<VkQueueFamilyProperties> mQueueFamiliesProperties;
        std::vector<VkExtensionProperties>   mExtensions;
        blk::PhysicalDeviceMemories          mMemories;

        explicit PhysicalDevice(VkPhysicalDevice vkphysicaldevice)
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
                mQueueFamiliesProperties.resize(count);
                vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, mQueueFamiliesProperties.data());
            }
            {// Extensions
                std::uint32_t count;
                CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, nullptr));
                mExtensions.resize(count);
                CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, mExtensions.data()));
            }
        }

        operator VkPhysicalDevice() const
        {
            return mPhysicalDevice;
        }
    };
}
