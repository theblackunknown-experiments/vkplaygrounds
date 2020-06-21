#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <vector>
#include <string_view>
#include <unordered_map>

struct VulkanApplication
{
    explicit VulkanApplication();
    ~VulkanApplication();

    struct {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
    } mVersion;
    std::vector<VkLayerProperties>                                           mLayers;
    std::vector<VkExtensionProperties>                                       mExtensions;
    std::unordered_map<std::string_view, std::vector<VkExtensionProperties>> mLayerExtensions;

    VkInstance               mInstance               = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebuggerMessenger      = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mStandardErrorMessenger = VK_NULL_HANDLE;
};
