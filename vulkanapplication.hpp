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

    VkInstance mInstance;

    VkDebugUtilsMessengerEXT mDebuggerMessenger;
    VkDebugUtilsMessengerEXT mStandardErrorMessenger;
};
