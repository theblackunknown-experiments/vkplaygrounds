#include <vulkan/vulkan.h>

#include <cassert>

#include <fstream>
#include <iostream>

#include <iterator>
#include <algorithm>

#include "vulkandebug.hpp"

#include "vulkandevice.hpp"

VulkanDevice::VulkanDevice(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device,
        std::uint32_t queue_family_index)
    : VulkanDeviceBase(device)
    , VulkanDeviceMixin()
    , mPhysicalDevice(physical_device)
    , mDevice(device)
    , mQueueFamilyIndex(queue_family_index)
{
}

VulkanDevice::~VulkanDevice()
{
    vkDestroyDevice(mDevice, nullptr);
}
