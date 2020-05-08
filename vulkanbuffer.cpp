#include <vulkan/vulkan.h>

#include "vulkandebug.hpp"

#include "vulkanbuffer.hpp"

#include <cassert>

VulkanBuffer::VulkanBuffer(
    VkDevice device,
    VkPhysicalDeviceMemoryProperties properties,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory)
    : mMemoryProperties(properties)
    , mDevice(device)
    , mUsageFlags(usage)
    , mMemoryFlags(memory)
{
}

void VulkanBuffer::allocate(VkDeviceSize size)
{
    assert(mMemory == VK_NULL_HANDLE);
    assert(mBuffer == VK_NULL_HANDLE);

    mSize = size;
    VkBufferCreateInfo info_buffer{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = mSize,
        .usage                 = mUsageFlags,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };
    CHECK(vkCreateBuffer(mDevice, &info_buffer, nullptr, &mBuffer));

    vkGetBufferMemoryRequirements(mDevice, mBuffer, &mRequirements);

    auto memory_type_index = get_memory_type(mRequirements.memoryTypeBits, mMemoryFlags);
    assert(memory_type_index.has_value());

    const VkMemoryAllocateInfo info_allocation{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = mRequirements.size,
        .memoryTypeIndex = memory_type_index.value(),
    };
    CHECK(vkAllocateMemory(mDevice, &info_allocation, nullptr, &mMemory));

    CHECK(vkBindBufferMemory(mDevice, mBuffer, mMemory, 0));

    mDescriptor.buffer = mBuffer;
    mDescriptor.offset = 0u;
    mDescriptor.range  = VK_WHOLE_SIZE;
}

void VulkanBuffer::deallocate()
{
    assert(mMemory != VK_NULL_HANDLE);
    assert(mBuffer != VK_NULL_HANDLE);

    vkDestroyBuffer(mDevice, mBuffer, nullptr);
    vkFreeMemory(mDevice, mMemory, nullptr);

    mMemory = VK_NULL_HANDLE;
    mBuffer = VK_NULL_HANDLE;

    mDescriptor.buffer = VK_NULL_HANDLE;
    mDescriptor.offset = 0u;
    mDescriptor.range  = 0u;
}

VulkanBuffer::~VulkanBuffer()
{
    vkDestroyBuffer(mDevice, mBuffer, nullptr);
    vkFreeMemory(mDevice, mMemory, nullptr);
}
