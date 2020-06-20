#pragma once

#include <vulkan/vulkan.h>

#include <cinttypes>

#include <vector>
#include <cassert>
#include <string_view>

#include "./vulkandebug.hpp"

struct [[deprecated("VulkanPhysicalDeviceBase -> VulkanPhysicalDeviceInformation")]] VulkanPhysicalDeviceBase
{
    explicit VulkanPhysicalDeviceBase(const VkPhysicalDevice& physical_device)
    {
        vkGetPhysicalDeviceFeatures(physical_device, &mFeatures);
        vkGetPhysicalDeviceProperties(physical_device, &mProperties);
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mMemoryProperties);
        {
            std::uint32_t count;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
            assert(count > 0);
            mQueueFamilies.resize(count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, mQueueFamilies.data());
        }
        {
            std::uint32_t count;
            CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr));
            mExtensions.resize(count);
            CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, mExtensions.data()));
        }
    }

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    std::vector<VkExtensionProperties>   mExtensions;
};
