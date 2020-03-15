#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>
#include <cinttypes>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "vulkandevicebase.hpp"
#include "vulkandevicemixin.hpp"

struct VulkanDevice
    : public VulkanDeviceBase
    , public VulkanDeviceMixin<VulkanDevice>
{
    explicit VulkanDevice(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device,
        std::uint32_t queue_family_index);
    ~VulkanDevice();

    VkCommandBuffer create_command_buffer(VkCommandBufferLevel level);
    void destroy_command_buffer(VkCommandBuffer command_buffer);

    VkShaderModule load_shader(const std::filesystem::path& filepath);

    VkPhysicalDevice mPhysicalDevice   = VK_NULL_HANDLE;
    VkDevice         mDevice           = VK_NULL_HANDLE;
    std::uint32_t    mQueueFamilyIndex = 0u;
};
