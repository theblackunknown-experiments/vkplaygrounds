
#include "./vkmemory.hpp"

#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

#include <cassert>

#include <numeric>
#include <algorithm>

namespace blk
{
    const MemoryType* PhysicalDeviceMemories::find_compatible(const Buffer& buffer, VkMemoryPropertyFlags flags) const
    {
        for (auto&& type : mTypes)
        {
            if (type.mBits & buffer.mRequirements.memoryTypeBits)
            {
                if (flags)
                {
                    if (type.supports(flags))
                    {
                        return std::addressof(type);
                    }
                }
                else
                {
                    return std::addressof(type);
                }
            }
        }
        return nullptr;
    }

    const MemoryType* PhysicalDeviceMemories::find_compatible(const Image& image, VkMemoryPropertyFlags flags) const
    {
        for (auto&& type : mTypes)
        {
            if (type.mBits & image.mRequirements.memoryTypeBits)
            {
                if (flags)
                {
                    if (type.supports(flags))
                    {
                        return std::addressof(type);
                    }
                }
                else
                {
                    return std::addressof(type);
                }
            }
        }
        return nullptr;
    }

    VkResult Memory::bind(const std::span<Buffer*>& buffers)
    {
        assert(mMemory != VK_NULL_HANDLE);

        std::vector<VkBindBufferMemoryInfo> infos;
        infos.reserve(buffers.size());

        const VkDeviceSize required_size = std::accumulate(
            std::begin(buffers), std::end(buffers),
            VkDeviceSize{0},
            [](VkDeviceSize current, const Buffer* buffer) {
                return current + buffer->mRequirements.size;
            }
        );
        assert(required_size <= mFree);

        std::transform(
            std::begin(buffers), std::end(buffers),
            std::back_inserter(infos),
            [this](Buffer* buffer) {
                auto info = VkBindBufferMemoryInfo{
                    .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                    .pNext        = nullptr,
                    .buffer       = buffer->mBuffer,
                    .memory       = mMemory,
                    .memoryOffset = mNextOffset,
                };
                buffer->mMemory = this;
                buffer->mOffset = mNextOffset;
                buffer->mOccupied = 0;

                mNextOffset += buffer->mRequirements.size;
                mFree       -= buffer->mRequirements.size;

                return info;
            }
        );
        auto result = vkBindBufferMemory2(mDevice, infos.size(), infos.data());
        CHECK(result);

        return result;
    }

    VkResult Memory::bind(const std::span<Image*>& images)
    {
        assert(mMemory != VK_NULL_HANDLE);

        std::vector<VkBindImageMemoryInfo> infos;
        infos.reserve(images.size());

        const VkDeviceSize required_size = std::accumulate(
            std::begin(images), std::end(images),
            VkDeviceSize{0},
            [](VkDeviceSize current, const Image* image) {
                return current + image->mRequirements.size;
            }
        );
        assert(required_size <= mFree);

        std::transform(
            std::begin(images), std::end(images),
            std::back_inserter(infos),
            [this](Image* image) {
                auto info = VkBindImageMemoryInfo{
                    .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                    .pNext        = nullptr,
                    .image        = image->mImage,
                    .memory       = mMemory,
                    .memoryOffset = mNextOffset,
                };
                image->mMemory   = this;
                image->mOffset   = mNextOffset;
                image->mOccupied = 0;

                mNextOffset += image->mRequirements.size;
                mFree       -= image->mRequirements.size;

                return info;
            }
        );
        auto result = vkBindImageMemory2(mDevice, infos.size(), infos.data());
        CHECK(result);

        return result;
    }
}
