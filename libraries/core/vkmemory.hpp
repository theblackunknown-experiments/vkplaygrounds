#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cinttypes>

#include <memory>

#include <span>
#include <vector>

#include "./vkdebug.hpp"

#include <core_export.h>

namespace blk
{
    struct Buffer;
    struct Image;

    struct MemoryHeap
    {
        VkMemoryHeap mHeap;

        constexpr bool supports(VkMemoryHeapFlags flags) const
        {
            return mHeap.flags & flags;
        }

        constexpr operator VkMemoryHeap() const
        {
            return mHeap;
        }
    };

    struct MemoryType
    {
        std::uint32_t mIndex;
        std::uint32_t mBits;
        VkMemoryType  mType;
        MemoryHeap*   mHeap;

        constexpr bool supports(VkMemoryPropertyFlags flags) const
        {
            return mType.propertyFlags & flags;
        }

        constexpr operator VkMemoryType() const
        {
            return mType;
        }
    };

    struct Memory
    {
        const MemoryType&    mType;
        VkMemoryAllocateInfo mInfo;
        VkDeviceMemory       mMemory = VK_NULL_HANDLE;
        VkDevice             mDevice = VK_NULL_HANDLE;

        std::uint32_t        mNextOffset = ~0;
        VkDeviceSize         mFree       = ~0;

        constexpr explicit Memory(const Memory& rhs) = delete;

        constexpr explicit Memory(Memory&& rhs)
            : mType      (rhs.mType)
            , mInfo      (std::move(rhs.mInfo))
            , mMemory    (std::exchange(rhs.mMemory, VkDeviceMemory{ VK_NULL_HANDLE }))
            , mDevice    (std::exchange(rhs.mDevice, VkDevice{ VK_NULL_HANDLE }))
            , mNextOffset(std::exchange(rhs.mNextOffset, ~0))
            , mFree      (std::exchange(rhs.mFree, ~0))
        {
        }

        constexpr explicit Memory(const MemoryType& type, VkDeviceSize size)
            : Memory(type, VkMemoryAllocateInfo{
                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext           = nullptr,
                .allocationSize  = size,
                .memoryTypeIndex = type.mIndex,
            })
        {
        }

        constexpr explicit Memory(const MemoryType& type, const VkMemoryAllocateInfo& info)
            : mType(type)
            , mInfo(info)
        {
        }

        ~Memory()
        {
            destroy();
        }

        Memory& operator=(const Memory& rhs) = delete;

        Memory& operator=(Memory&& rhs)
        {
            assert(std::addressof(mType) == std::addressof(rhs.mType));
            mInfo       = std::exchange(rhs.mInfo      , mInfo);
            mMemory     = std::exchange(rhs.mMemory    , mMemory);
            mDevice     = std::exchange(rhs.mDevice    , mDevice);
            mNextOffset = std::exchange(rhs.mNextOffset, mNextOffset);
            mFree       = std::exchange(rhs.mFree      , mFree      );
            return *this;
        }

        VkResult allocate(VkDevice vkdevice)
        {
            mDevice = vkdevice;
            auto result = vkAllocateMemory(mDevice, &mInfo, nullptr, &mMemory);
            CHECK(result);
            mNextOffset = 0;
            mFree = mInfo.allocationSize;
            return result;
        }

        VkResult reallocate(VkDevice vkdevice, VkDeviceSize size)
        {
            assert((mDevice == VK_NULL_HANDLE) || (mDevice == vkdevice));

            if (mInfo.allocationSize >= size)
                return VK_SUCCESS;

            destroy();

            mDevice = vkdevice;
            mInfo.allocationSize = size;

            auto result = vkAllocateMemory(mDevice, &mInfo, nullptr, &mMemory);
            CHECK(result);
            mNextOffset = 0;
            mFree = mInfo.allocationSize;
            return result;
        }

        void destroy()
        {
            if (mMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(mDevice, mMemory, nullptr);
                mMemory = VK_NULL_HANDLE;
            }
        }

        CORE_EXPORT VkResult bind(Buffer& buffer);
        CORE_EXPORT VkResult bind(const std::span<Buffer*>& buffers);
        CORE_EXPORT VkResult bind(const std::initializer_list<Buffer*>& buffers);
        CORE_EXPORT VkResult bind(Image& image);
        CORE_EXPORT VkResult bind(const std::span<Image*>& images);
        CORE_EXPORT VkResult bind(const std::initializer_list<Image*>& images);

        constexpr operator VkDeviceMemory() const
        {
            return mMemory;
        }
    };

    struct PhysicalDeviceMemories
    {
        VkPhysicalDeviceMemoryProperties mProperties;
        std::vector<MemoryHeap>          mHeaps;
        std::vector<MemoryType>          mTypes;

        void initialize(const VkPhysicalDeviceMemoryProperties& properties)
        {
            mProperties = properties;
            mHeaps.reserve(properties.memoryHeapCount);
            for(std::uint32_t idx = 0; idx < properties.memoryHeapCount; ++idx)
            {
                mHeaps.push_back(MemoryHeap{properties.memoryHeaps[idx]});
            }
            mTypes.reserve(properties.memoryTypeCount);
            for(std::uint32_t idx = 0; idx < properties.memoryTypeCount; ++idx)
            {
                const VkMemoryType& type(properties.memoryTypes[idx]);
                mTypes.emplace_back(MemoryType{idx, 1u << idx, type, &mHeaps.at(type.heapIndex)});
            }
        }

        CORE_EXPORT const MemoryType* find_compatible(const Buffer& buffer, VkMemoryPropertyFlags flags = 0) const;
        CORE_EXPORT const MemoryType* find_compatible(const Image& image, VkMemoryPropertyFlags flags = 0) const;
    };
}
