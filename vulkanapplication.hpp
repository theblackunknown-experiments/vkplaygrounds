#pragma once

#include <vulkan/vulkan.h>

#include "vulkanapplicationbase.hpp"
#include "vulkaninstancemixin.hpp"

struct VulkanApplication
    : public VulkanApplicationBase
    , public VulkanInstanceMixin<VulkanApplication>
{
    explicit VulkanApplication();
    ~VulkanApplication();

    VkInstance mInstance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebuggerMessenger = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mStandardErrorMessenger = VK_NULL_HANDLE;
};
