#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <memory>

#include <span>
#include <vector>

#include "./vkdebug.hpp"

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
            if (mMemory != VK_NULL_HANDLE)
                vkFreeMemory(mDevice, mMemory, nullptr);
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

        VkResult bind(const std::span<Buffer*>& buffers);
        VkResult bind(const std::span<Image*>& images);

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

        explicit PhysicalDeviceMemories()
        {
        }

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

        const MemoryType* find_compatible(const Buffer& buffer, VkMemoryPropertyFlags flags = 0) const;
        const MemoryType* find_compatible(const Image& image, VkMemoryPropertyFlags flags = 0) const;
    };
}
