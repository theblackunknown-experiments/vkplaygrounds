#pragma once

#include <vulkan/vulkan.h>

#include <cinttypes>

#include <iterator>
#include <algorithm>

#include <optional>

template<typename Base>
struct VulkanPhysicalDeviceMixin
{
    bool has_extension(const std::string_view& extension) const
    {
        const Base& that = static_cast<const Base&>(*this);
        return std::find_if(
            std::begin(that.mExtensions), std::end(that.mExtensions),
            [&extension](const auto& e){
                return extension == e.extensionName;
            }
        ) != std::end(that.mExtensions);
    }

    std::optional<std::uint32_t> get_memory_type(std::uint32_t type_bits, VkMemoryPropertyFlags flags) const
    {
        const Base& that = static_cast<const Base&>(*this);
        for(std::uint32_t idx = 0; idx < that.mMemoryProperties.memoryTypeCount; ++idx, type_bits >>= 1)
        {
            if((type_bits & 1) == 1)
            {
                const VkMemoryType& memory_type = that.mMemoryProperties.memoryTypes[idx];
                if((memory_type.propertyFlags & flags) == flags)
                    return idx;
            }
        }
        return {};
    }

    std::optional<std::uint32_t> select_queue_family_index(VkQueueFlags requirements) const
    {
        const Base& that = static_cast<const Base&>(*this);
        // Find a dedicated queue which match the requirement, or fallback on a compatible one
        auto finder_dedicated = std::find_if(
            std::begin(that.mQueueFamilies), std::end(that.mQueueFamilies),
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
        if (finder_dedicated == std::end(that.mQueueFamilies))
        {
            auto finder_compatible = std::find_if(
                std::begin(that.mQueueFamilies), std::end(that.mQueueFamilies),
                [&requirements](const VkQueueFamilyProperties& p) {
                    return (p.queueFlags & requirements) == requirements;
                }
            );
            finder_dedicated = finder_compatible;
        }
        if (finder_dedicated != std::end(that.mQueueFamilies))
        {
            return static_cast<std::uint32_t>(std::distance(std::begin(that.mQueueFamilies), finder_dedicated));
        }
        else
        {
            return { };
        }
    }

    std::optional<VkFormat> select_best_depth_format() const
    {
        const Base& that = static_cast<const Base&>(*this);
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
            [&that](const VkFormat& f) {
                VkFormatProperties properties;
                vkGetPhysicalDeviceFormatProperties(that.mPhysicalDevice, f, &properties);
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
};
