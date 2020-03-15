#pragma once

#include <vulkan/vulkan.h>

#include <cinttypes>

#include <vector>
#include <string_view>
#include <unordered_map>

struct VulkanApplicationBase
{
    explicit VulkanApplicationBase()
    {
        { // Version
            std::uint32_t version;
            CHECK(vkEnumerateInstanceVersion(&version));
            mVersion.major = VK_VERSION_MAJOR(version);
            mVersion.minor = VK_VERSION_MINOR(version);
            mVersion.patch = VK_VERSION_PATCH(version);
        }
        { // Layers / Extensions
            std::uint32_t count;
            CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
            mLayers.resize(count);
            CHECK(vkEnumerateInstanceLayerProperties(&count, mLayers.data()));
        }
        {
            std::uint32_t count;
            CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
            mExtensions.resize(count);
            CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, mExtensions.data()));

            for (auto&& layer_properties : mLayers)
            {
                CHECK(vkEnumerateInstanceExtensionProperties(layer_properties.layerName, &count, nullptr));
                auto& layer_extensions = mLayerExtensions[layer_properties.layerName];
                layer_extensions.resize(count);
                CHECK(vkEnumerateInstanceExtensionProperties(layer_properties.layerName, &count, layer_extensions.data()));
            }
        }
    }

    struct {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
    } mVersion;
    std::vector<VkLayerProperties> mLayers;
    std::vector<VkExtensionProperties> mExtensions;
    std::unordered_map<std::string_view, std::vector<VkExtensionProperties>> mLayerExtensions;
};

