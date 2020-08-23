#pragma once

#include <cinttypes>

#include <vulkan/vulkan.h>

struct VulkanBuffer
{
    VulkanBuffer(
        VkDevice device,
        VkPhysicalDeviceMemoryProperties properties,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory);

    ~VulkanBuffer();

    void allocate(VkDeviceSize size);
    void deallocate();

    VkDevice                         mDevice       = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties mMemoryProperties ;
    VkBufferUsageFlags               mUsageFlags ;
    VkMemoryPropertyFlags            mMemoryFlags;


    VkDeviceSize           mSize         = VK_NULL_HANDLE;
    VkDeviceMemory         mMemory       = VK_NULL_HANDLE;
    VkBuffer               mBuffer       = VK_NULL_HANDLE;
    VkMemoryRequirements   mRequirements = { };
    VkDescriptorBufferInfo mDescriptor   = { };
};
