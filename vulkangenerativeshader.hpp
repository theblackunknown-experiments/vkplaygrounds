#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <tuple>

struct VulkanGenerativeShader
{
    explicit VulkanGenerativeShader(
        VkPhysicalDevice vkphysicaldevice,
        std::uint32_t queue_family_index,
        std::uint32_t queue_count
    );
    ~VulkanGenerativeShader();

    static
    std::tuple<VkPhysicalDevice, std::uint32_t, std::uint32_t> requirements(const std::span<VkPhysicalDevice>& vkphysicaldevices);

    // Setup

    // States

    VkPhysicalDevice                     mPhysicalDevice  = VK_NULL_HANDLE;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamiliesProperties;
    std::vector<VkExtensionProperties>   mExtensions;

    std::uint32_t                        mQueueFamilyIndex;
    VkDevice                             mDevice          = VK_NULL_HANDLE;
    VkQueue                              mQueue           = VK_NULL_HANDLE;
};
