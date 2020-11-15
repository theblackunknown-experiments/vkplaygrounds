#pragma once

#include "./vkdebug.hpp"

#include <vulkan/vulkan_core.h>

#include <cinttypes>

namespace blk
{
    struct Memory;

    struct Buffer
    {
        VkBufferCreateInfo   mInfo;
        VkBuffer             mBuffer       = VK_NULL_HANDLE;
        VkDevice             mDevice       = VK_NULL_HANDLE;
        VkMemoryRequirements mRequirements;

        Memory*              mMemory       = nullptr;
        std::uint32_t        mOffset       = ~0;
        std::uint32_t        mOccupied     = ~0;

        constexpr Buffer() = default;

        constexpr Buffer(const Buffer& rhs) = delete;

        constexpr Buffer(Buffer&& rhs)
            : mInfo(std::move(rhs.mInfo))
            , mBuffer(std::exchange(rhs.mBuffer, VkBuffer{VK_NULL_HANDLE}))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
            , mRequirements(std::move(rhs.mRequirements))
            , mMemory(std::exchange(rhs.mMemory, nullptr))
            , mOffset(std::exchange(rhs.mOffset, ~0))
            , mOccupied(std::exchange(rhs.mOccupied, ~0))
        {
        }

        constexpr explicit Buffer(const VkDeviceSize& size, VkBufferUsageFlags flags)
            : Buffer(VkBufferCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .size                  = size,
                .usage                 = flags,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
            })
        {
        }

        constexpr explicit Buffer(const VkBufferCreateInfo& info)
            : mInfo(info)
        {
        }

        ~Buffer()
        {
            destroy();
        }

        Buffer& operator=(const Buffer& rhs) = delete;

        Buffer& operator=(Buffer&& rhs)
        {
            mInfo         = std::exchange(rhs.mInfo        , mInfo);
            mBuffer       = std::exchange(rhs.mBuffer      , mBuffer);
            mDevice       = std::exchange(rhs.mDevice      , mDevice);
            mRequirements = std::exchange(rhs.mRequirements, mRequirements);
            mMemory       = std::exchange(rhs.mMemory      , mMemory);
            mOffset       = std::exchange(rhs.mOffset      , mOffset);
            mOccupied     = std::exchange(rhs.mOccupied    , mOccupied);
            return *this;
        }

        VkResult create(VkDevice vkdevice)
        {
            mDevice = vkdevice;
            auto result = vkCreateBuffer(mDevice, &mInfo, nullptr, &mBuffer);
            CHECK(result);
            vkGetBufferMemoryRequirements(mDevice, mBuffer, &mRequirements);
            return result;
        }

        void destroy()
        {
            if (mBuffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(mDevice, mBuffer, nullptr);
                mBuffer   = VK_NULL_HANDLE;
                mMemory   = nullptr;
                mOffset   = ~0;
                mOccupied = ~0;
            }
        }

        constexpr bool created() const
        {
            return mBuffer != VK_NULL_HANDLE;
        }

        constexpr bool bound() const
        {
            return mOffset != (std::uint32_t)~0;
        }

        constexpr operator VkBuffer() const
        {
            return mBuffer;
        }
    };
}
