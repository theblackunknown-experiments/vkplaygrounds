#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>
#include <cinttypes>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "vulkanphysicaldevicebase.hpp"
#include "vulkanphysicaldevicemixin.hpp"

struct VulkanPhysicalDevice
    : public VulkanPhysicalDeviceBase
    , public VulkanPhysicalDeviceMixin<VulkanPhysicalDevice>
{
    explicit VulkanPhysicalDevice(const VkPhysicalDevice& physical_device);
    ~VulkanPhysicalDevice();

    std::tuple<std::uint32_t, VkDevice> create_device(VkQueueFlags queue_requirements, bool swap_chain);

    struct {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
    } mVersion;

    struct {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;
    } mDriverVersion;

    VkPhysicalDevice mPhysicalDevice;
};
