#pragma once

#include <vulkan/vulkan.h>

#include <cinttypes>

#include <iterator>
#include <algorithm>

#include <string_view>

template<typename Base>
struct VulkanInstanceMixin
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

    std::vector<VkPhysicalDevice> physical_devices() const
    {
        const Base& that = static_cast<const Base&>(*this);

        std::uint32_t count = 0;
        CHECK(vkEnumeratePhysicalDevices(that.mInstance, &count, nullptr));
        assert(count > 0);

        std::vector<VkPhysicalDevice> physical_devices(count);
        CHECK(vkEnumeratePhysicalDevices(that.mInstance, &count, physical_devices.data()));
        return physical_devices;
    }
};

