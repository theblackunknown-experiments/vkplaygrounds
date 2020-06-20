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

    [[nodiscard]]
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
};
