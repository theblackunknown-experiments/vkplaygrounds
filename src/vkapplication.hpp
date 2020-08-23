#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <vector>
#include <string_view>
#include <unordered_map>

#include "./vkutilities.hpp"

struct VulkanApplication
{
    explicit VulkanApplication(Version version);
    ~VulkanApplication();

    operator VkInstance() const
    {
        return mInstance;
    }

    Version mVersion;
    std::vector<VkLayerProperties>                                           mLayers;
    std::vector<VkExtensionProperties>                                       mExtensions;
    std::unordered_map<std::string_view, std::vector<VkExtensionProperties>> mLayerExtensions;

    VkInstance               mInstance               = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebuggerMessenger      = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mStandardErrorMessenger = VK_NULL_HANDLE;
};
