#pragma once

#include <vulkan/vulkan.h>

#include "vulkandebug.hpp"
#include "vulkanbuffer.hpp"

template<typename T>
struct ScopedBufferMapping
{
    ScopedBufferMapping(VulkanBuffer& buffer, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
        : mBuffer(buffer)
    {
        void * mapped_memory = nullptr;
        CHECK(vkMapMemory(mBuffer.mDevice, mBuffer.mMemory, offset, size, 0, &mapped_memory));
        mMappedMemory = reinterpret_cast<T*>(mapped_memory);
    }

    ~ScopedBufferMapping()
    {
        vkUnmapMemory(mBuffer.mDevice, mBuffer.mMemory);
    }

    VulkanBuffer& mBuffer;
    T* mMappedMemory = nullptr;
};
