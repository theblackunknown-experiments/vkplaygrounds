#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <span>
#include <vector>
#include <string_view>

#include <iterator>
#include <optional>
#include <algorithm>

#include "./vkdebug.hpp"

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if __has_include(<bit>)
// #  pragma message( __FILE__ "(" STRING(__LINE__) "): <bits> available"  )
#  include <bit>
#  define HAVE_HEADER_BIT 1
#else
// #  pragma message( __FILE__ "(" STRING(__LINE__) "): <bits> *NOT* available"  )
#  define HAVE_HEADER_BIT 0
#endif

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

struct PresentationData {
    VkSwapchainKHR               swapchain     = VK_NULL_HANDLE;
    std::uint32_t                count         = {};
    std::vector<VkImage>         images        ;
    std::vector<VkImageView>     views         ;
    std::vector<VkCommandBuffer> commandbuffers;
    std::vector<VkFence>         wait_fences   ;
};

struct Mouse
{
    struct Buttons
    {
        bool left   = false;
        bool middle = false;
        bool right  = false;
    } buttons;
    struct Positions
    {
        float x = 0.0f;
        float y = 0.0f;
    } offset;
};

inline
constexpr std::uint32_t next_pow2_dummy(std::uint32_t v)
{
    std::uint32_t p = 1;
    while (p < v) p *= 2;
    return p;
}

inline
constexpr std::uint32_t next_pow2_portable(std::uint32_t v)
{
    --v;
    v |= v >>  1;
    v |= v >>  2;
    v |= v >>  4;
    v |= v >>  8;
    v |= v >> 16;
    ++v;
    return v;
}

#if HAVE_HEADER_BIT
inline
constexpr std::uint32_t next_pow2_modern(std::uint32_t v)
{
    return v == 1
        ? v
        : 1 << (32 - std::countl_zero(v - 1));
    --v;
    v |= v >>  1;
    v |= v >>  2;
    v |= v >>  4;
    v |= v >>  8;
    v |= v >> 16;
    ++v;
    return v;
}

inline
constexpr std::uint32_t next_pow2_cpp20(std::uint32_t v)
{
    return std::bit_ceil(v);
}
#endif

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

[[nodiscard]]
inline
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

[[nodiscard]]
inline
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
inline
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

[[nodiscard]]
inline
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

namespace blk
{

struct PhysicalDevice;

[[nodiscard]]
std::vector<blk::PhysicalDevice> physicaldevices(VkInstance instance);

}
