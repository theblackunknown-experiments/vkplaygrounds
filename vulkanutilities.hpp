#pragma once

#include <vulkan/vulkan.h>

#include <cassert>
#include <cinttypes>

#include <span>
#include <vector>
#include <iterator>
#include <optional>
#include <algorithm>

#include <string_view>

#include "./vulkandebug.hpp"

inline
bool has_extension(const std::span<VkExtensionProperties>& extensions, const std::string_view& extension)
{
    return std::find_if(
        std::begin(extensions), std::end(extensions),
        [&extension](const auto& e){
            return extension == e.extensionName;
        }
    ) != std::end(extensions);
}

inline
[[nodiscard]]
std::vector<VkPhysicalDevice> physical_devices(VkInstance instance)
{
    std::uint32_t count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    assert(count > 0);

    std::vector<VkPhysicalDevice> physical_devices(count);
    CHECK(vkEnumeratePhysicalDevices(instance, &count, physical_devices.data()));
    return physical_devices;
}


inline
[[nodiscard]]
std::optional<std::uint32_t> select_queue_family_index(
    const std::span<VkQueueFamilyProperties>& queue_families_properties,
    VkQueueFlags requirements)
{
    // Find a dedicated queue which match the requirement, or fallback on a compatible one
    auto finder_dedicated = std::find_if(
        std::begin(queue_families_properties), std::end(queue_families_properties),
        [&requirements](const VkQueueFamilyProperties& p) {
            if (requirements & VK_QUEUE_GRAPHICS_BIT)
            {
                return (p.queueFlags & requirements)
                    && ((p.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
                    && ((p.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0);
            }
            else if (requirements & VK_QUEUE_COMPUTE_BIT)
            {
                return (p.queueFlags & requirements)
                    && ((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                    && ((p.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0);
            }
            else if (requirements & VK_QUEUE_TRANSFER_BIT)
            {
                return (p.queueFlags & requirements)
                    && ((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                    && ((p.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0);
            }
            else
            {
                return false;
            }
        }
    );
    if (finder_dedicated == std::end(queue_families_properties))
    {
        auto finder_compatible = std::find_if(
            std::begin(queue_families_properties), std::end(queue_families_properties),
            [&requirements](const VkQueueFamilyProperties& p) {
                return (p.queueFlags & requirements) == requirements;
            }
        );
        finder_dedicated = finder_compatible;
    }
    if (finder_dedicated != std::end(queue_families_properties))
    {
        return static_cast<std::uint32_t>(std::distance(std::begin(queue_families_properties), finder_dedicated));
    }
    else
    {
        return { };
    }
}

[[nodiscard]]
std::optional<std::uint32_t> select_surface_queue_family_index(
    VkPhysicalDevice vkphysicaldevice,
    const std::span<VkQueueFamilyProperties>& queue_families_properties,
    VkSurfaceKHR surface)
{
    std::uint32_t count = queue_families_properties.size();
    std::vector<VkBool32> supporteds(count);
    std::optional<std::uint32_t> selected_queue;
    for (std::uint32_t idx = 0; idx < count; ++idx)
    {
        VkBool32 supported;
        CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, idx, surface, &supported));
        supporteds.at(idx) = supported;

        const VkQueueFamilyProperties& p = queue_families_properties[idx];
        if((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (supported == VK_TRUE))
        {
            selected_queue = idx;
            break;
        }
    }
    if (!selected_queue.has_value())
    {
        for(std::uint32_t idx = 0; idx < count; ++idx)
        {
            const VkBool32& supported = supporteds[idx];
            if(supported == VK_TRUE)
            {
                selected_queue = idx;
                break;
            }
        }
    }
    return selected_queue;
}

inline
[[nodiscard]]
std::optional<VkFormat> select_best_depth_format(VkPhysicalDevice vkphysicaldevice)
{
    // Depth Format
    constexpr const VkFormat kDEPTH_FORMATS[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    auto finder_depth_format = std::find_if(
        std::begin(kDEPTH_FORMATS), std::end(kDEPTH_FORMATS),
        [&vkphysicaldevice](const VkFormat& f) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(vkphysicaldevice, f, &properties);
            return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
    );

    if (finder_depth_format != std::end(kDEPTH_FORMATS))
    {
        return *finder_depth_format;
    }
    else
    {
        return { };
    }
}

inline
[[nodiscard]]
const char* DeviceType2Text(const VkPhysicalDeviceType& type)
{
    switch(type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER         : return "OTHER";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU  : return "DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU   : return "VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU           : return "CPU";
        default                                    : return "Unknown";
    }
}
