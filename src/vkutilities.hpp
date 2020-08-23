#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <span>
#include <vector>
#include <iterator>
#include <optional>
#include <algorithm>

#include <string_view>

#include "./vkdebug.hpp"

struct Version
{
    std::uint32_t mPacked;

    operator std::uint32_t() const
    {
        return mPacked;
    }

    std::uint32_t major() const
    {
        return VK_VERSION_MAJOR(mPacked);
    }

    std::uint32_t minor() const
    {
        return VK_VERSION_MINOR(mPacked);
    }

    std::uint32_t patch() const
    {
        return VK_VERSION_PATCH(mPacked);
    }
};

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
std::optional<std::uint32_t> get_memory_type(const VkPhysicalDeviceMemoryProperties& properties, std::uint32_t type_bits, VkMemoryPropertyFlags flags)
{
    for(std::uint32_t idx = 0; idx < properties.memoryTypeCount; ++idx, type_bits >>= 1)
    {
        if((type_bits & 1) == 1)
        {
            const VkMemoryType& memory_type = properties.memoryTypes[idx];
            if((memory_type.propertyFlags & flags) == flags)
                return idx;
        }
    }
    return { };
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

inline
[[nodiscard]]
VkCommandBuffer create_command_buffer(VkDevice vkdevice, VkCommandPool vkcmdpool, VkCommandBufferLevel level)
{
    VkCommandBuffer vkcmdbuffer;
    const VkCommandBufferAllocateInfo info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = vkcmdpool,
        .level              = level,
        .commandBufferCount = 1,
    };
    CHECK(vkAllocateCommandBuffers(vkdevice, &info, &vkcmdbuffer));
    return vkcmdbuffer;
}

inline
void destroy_command_buffer(VkDevice vkdevice, VkCommandPool vkcmdpool, VkCommandBuffer vkcmdbuffer)
{
    vkFreeCommandBuffers(vkdevice, vkcmdpool, 1, &vkcmdbuffer);
}
