#include <vulkan/vulkan.h>

#include <span>
#include <iterator>
#include <iostream>

#include "./vkdebug.hpp"
#include "./vkutilities.hpp"

#include "./vulkangenerativeshader.hpp"

bool VulkanGenerativeShader::supports(VkPhysicalDevice vkphysicaldevice)
{
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

    auto optional_queue_family_index = select_queue_family_index(queue_families, VK_QUEUE_COMPUTE_BIT);

    return optional_queue_family_index.has_value();
}

std::tuple<std::uint32_t, std::uint32_t> VulkanGenerativeShader::requirements(VkPhysicalDevice vkphysicaldevice)
{
    constexpr const std::uint32_t queue_count = 1;
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

    auto optional_queue_family_index = select_queue_family_index(queue_families, VK_QUEUE_COMPUTE_BIT);

    if (optional_queue_family_index.has_value())
        return std::make_tuple(optional_queue_family_index.value(), queue_count);

    std::cerr << "No valid physical device supports VulkanGenerativeShader" << std::endl;
    return std::make_tuple(0u, 0u);
}

VulkanGenerativeShader::VulkanGenerativeShader(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues
    )
    : mPhysicalDevice(vkphysicaldevice)
    , mDevice(vkdevice)
    , mQueueFamilyIndex(queue_family_index)
    , mQueue(vkqueues[0])
{
    assert(vkqueues.size() == 1);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

    {// Command Pool
        VkCommandPoolCreateInfo info_pool{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = mQueueFamilyIndex,
        };
        CHECK(vkCreateCommandPool(mDevice, &info_pool, nullptr, &mCommandPool));
    }
}

VulkanGenerativeShader::~VulkanGenerativeShader()
{
    vkDestroyDevice(mDevice, nullptr);
}
