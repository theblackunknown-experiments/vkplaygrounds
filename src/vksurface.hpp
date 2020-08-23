#pragma once

#include <vulkan/vulkan_core.h>

struct VulkanSurface
{
    explicit VulkanSurface(VkInstance instance, VkSurfaceKHR surface);
    ~VulkanSurface();

    operator VkSurfaceKHR() const
    {
        return mSurface;
    }

    VkInstance   mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
};
