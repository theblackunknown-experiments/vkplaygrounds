#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <span>
#include <tuple>
#include <vector>

struct VulkanGenerativeShader
{
    explicit VulkanGenerativeShader(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues
    );
    ~VulkanGenerativeShader();

    static
    bool supports(VkPhysicalDevice vkphysicaldevice);

    static
    std::tuple<std::uint32_t, std::uint32_t> requirements(VkPhysicalDevice vkphysicaldevice);

    // Setup

    // States

    VkPhysicalDevice                     mPhysicalDevice  = VK_NULL_HANDLE;
    std::uint32_t                        mQueueFamilyIndex;
    VkDevice                             mDevice          = VK_NULL_HANDLE;
    VkQueue                              mQueue           = VK_NULL_HANDLE;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;

    VkCommandPool                        mCommandPool = VK_NULL_HANDLE;
};
